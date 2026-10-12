#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <BluetoothSerial.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "esp32-hal-bt.h"
#include "esp_gap_bt_api.h"

#include "HourHistory.h"
#include "SensorHistory.h"
#include "index_html.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth Classic is not enabled
#endif

namespace {

constexpr uint32_t kSampleIntervalMs = 1000;
constexpr uint32_t kWifiRetryMs = 10000;
constexpr uint32_t kProvisionTimeoutMs = 15000;
constexpr uint32_t kProvisionArmDelayMs = 100;
constexpr uint32_t kBootDebounceMs = 50;
constexpr uint32_t kLedBlinkHalfMs = 125;
constexpr uint32_t kBtStopDelayMs = 200;
constexpr uint32_t kBtTaskStackBytes = 20480;
constexpr uint8_t kBootPin = 0;
constexpr uint8_t kLedPin = LED_BUILTIN;
constexpr size_t kSsidMax = 32;
constexpr size_t kPassMax = 63;
constexpr char kHostname[] = "terrarium";
constexpr char kBtName[] = "Terrarium";
constexpr char kNvsNamespace[] = "terrarium";
constexpr char kTimeZone[] = "MSK-3";
constexpr uint32_t kHourPersistMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kHourSaveRetryMs = 30000;
constexpr int kClockValidYear = 2016;

enum class RunMode : uint8_t {
  Idle,
  ConnectingSaved,
  BtListening,
  WifiProvisioning,
  Streaming,
};

Adafruit_AHTX0 aht;
WebServer server(80);
SensorHistory history;
HourHistory hourHistory;
BluetoothSerial serialBt;

char jsonBuf[4096];
bool ntpStarted = false;
bool liveValid = false;
float liveTemperature = 0;
float liveHumidity = 0;
time_t trackedHour = 0;
bool hourSaveSoon = false;
bool hourSaveFailed = false;
uint32_t lastHourSaveMs = 0;
uint32_t lastHourSaveAttemptMs = 0;
uint32_t lastSampleMs = 0;
uint32_t lastWifiAttemptMs = 0;
bool wifiWasConnected = false;
bool serverStarted = false;
bool mdnsStarted = false;

char savedSsid[kSsidMax + 1] = {};
char savedPass[kPassMax + 1] = {};
char pendingSsid[kSsidMax + 1] = {};
char pendingPass[kPassMax + 1] = {};
bool hasSavedCreds = false;
bool wifiSuspended = false;
bool savedBeginPending = false;
uint32_t savedBeginRequestMs = 0;

RunMode mode = RunMode::Idle;
bool btOn = false;
bool btStackReady = false;
bool btStopRequested = false;
uint32_t btStopRequestMs = 0;
char btLine[128];
size_t btLineLen = 0;
bool btLineOverflow = false;

bool provisionArmed = false;
uint32_t provisionStartMs = 0;
uint32_t provisionEpoch = 0;
volatile uint32_t wifiEpoch = 0;
volatile uint32_t gotIpEpoch = 0;

bool bootRawHigh = true;
bool bootStableHigh = true;
uint32_t bootChangeMs = 0;

bool appendFmt(char*& cursor, char* end, const char* fmt, ...) {
  if (cursor >= end) {
    return false;
  }

  va_list args;
  va_start(args, fmt);
  const int written = vsnprintf(cursor, static_cast<size_t>(end - cursor), fmt, args);
  va_end(args);

  if (written < 0 || written >= end - cursor) {
    if (end > cursor) {
      *cursor = '\0';
    }
    cursor = end;
    return false;
  }

  cursor += written;
  return true;
}

bool clockIsSynced() {
  time_t now = 0;
  time(&now);
  struct tm local = {};
  if (localtime_r(&now, &local) == nullptr) {
    return false;
  }
  return local.tm_year > (kClockValidYear - 1900);
}

time_t currentHourStart() {
  if (!clockIsSynced()) {
    return 0;
  }

  time_t now = 0;
  time(&now);
  struct tm local = {};
  if (localtime_r(&now, &local) == nullptr) {
    return 0;
  }
  local.tm_min = 0;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  const time_t start = mktime(&local);
  if (start <= 0) {
    return 0;
  }
  return start;
}

void maintainClock() {
  if (WiFi.status() != WL_CONNECTED || ntpStarted) {
    return;
  }
  configTzTime(kTimeZone, "pool.ntp.org", "time.nist.gov");
  ntpStarted = true;
  Serial.println("NTP started");
}

void persistHourHistory() {
  const time_t hour = currentHourStart();
  if (hour > 0) {
    if (trackedHour != 0 && hour != trackedHour) {
      hourSaveSoon = true;
    }
    trackedHour = hour;
  }

  if (!hourHistory.dirty()) {
    hourSaveSoon = false;
    return;
  }

  const uint32_t now = millis();
  if (!hourSaveSoon && now - lastHourSaveMs < kHourPersistMs) {
    return;
  }
  if (hourSaveFailed && now - lastHourSaveAttemptMs < kHourSaveRetryMs) {
    return;
  }

  lastHourSaveAttemptMs = now;
  if (!hourHistory.save(kNvsNamespace)) {
    hourSaveFailed = true;
    Serial.println("Hour history save failed");
    return;
  }
  hourSaveFailed = false;
  hourSaveSoon = false;
  lastHourSaveMs = now;
  Serial.println("Hour history saved");
}

bool appendLive(char*& cursor, char* end, float value, const char* format) {
  if (!liveValid) {
    return appendFmt(cursor, end, "null");
  }
  return appendFmt(cursor, end, format, static_cast<double>(value));
}

bool appendSeries(char*& cursor, char* end, bool temperatureSeries) {
  for (size_t i = 0; i < SensorHistory::kCapacity; ++i) {
    if (i > 0 && !appendFmt(cursor, end, ",")) {
      return false;
    }

    float temperature = 0;
    float humidity = 0;
    if (!history.at(i, temperature, humidity)) {
      if (!appendFmt(cursor, end, "null")) {
        return false;
      }
      continue;
    }

    const float value = temperatureSeries ? temperature : humidity;
    const char* format = temperatureSeries ? "%.2f" : "%.1f";
    if (!appendFmt(cursor, end, format, static_cast<double>(value))) {
      return false;
    }
  }
  return true;
}

bool appendHourSeries(char*& cursor, char* end, time_t currentHour, bool temperatureSeries) {
  for (size_t i = 0; i < HourHistory::kCapacity; ++i) {
    if (i > 0 && !appendFmt(cursor, end, ",")) {
      return false;
    }

    const HourHistory::Point point = hourHistory.at(i, currentHour);
    if (!point.valid) {
      if (!appendFmt(cursor, end, "null")) {
        return false;
      }
      continue;
    }

    const float value = temperatureSeries ? point.temperature : point.humidity;
    const char* format = temperatureSeries ? "%.2f" : "%.1f";
    if (!appendFmt(cursor, end, format, static_cast<double>(value))) {
      return false;
    }
  }
  return true;
}

bool appendHourLabels(char*& cursor, char* end, time_t currentHour) {
  for (size_t i = 0; i < HourHistory::kCapacity; ++i) {
    if (i > 0 && !appendFmt(cursor, end, ",")) {
      return false;
    }

    char label[16] = {};
    if (currentHour > 0) {
      const time_t age = static_cast<time_t>(HourHistory::kCapacity - 1 - i);
      const time_t want = currentHour - age * 3600;
      struct tm local = {};
      if (localtime_r(&want, &local) != nullptr) {
        strftime(label, sizeof(label), "%d.%m %H:%M", &local);
      }
    }
    if (!appendFmt(cursor, end, "\"%s\"", label)) {
      return false;
    }
  }
  return true;
}

const char* buildHistoryJson() {
  const time_t currentHour = currentHourStart();
  char* cursor = jsonBuf;
  char* end = jsonBuf + sizeof(jsonBuf);
  if (!appendFmt(cursor, end, "{\"synced\":%s,\"live_temperature\":",
                 currentHour > 0 ? "true" : "false") ||
      !appendLive(cursor, end, liveTemperature, "%.2f") ||
      !appendFmt(cursor, end, ",\"live_humidity\":") ||
      !appendLive(cursor, end, liveHumidity, "%.1f") ||
      !appendFmt(cursor, end, ",\"temperature\":[") ||
      !appendSeries(cursor, end, true) ||
      !appendFmt(cursor, end, "],\"humidity\":[") ||
      !appendSeries(cursor, end, false) ||
      !appendFmt(cursor, end, "],\"history_labels\":[") ||
      !appendHourLabels(cursor, end, currentHour) ||
      !appendFmt(cursor, end, "],\"history_temperature\":[") ||
      !appendHourSeries(cursor, end, currentHour, true) ||
      !appendFmt(cursor, end, "],\"history_humidity\":[") ||
      !appendHourSeries(cursor, end, currentHour, false) ||
      !appendFmt(cursor, end, "]}")) {
    return nullptr;
  }
  return jsonBuf;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleHistory() {
  const char* json = buildHistoryJson();
  if (json == nullptr) {
    server.send(500, "application/json", "{\"error\":\"buffer\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void sampleSensor() {
  const uint32_t now = millis();
  if (now - lastSampleMs < kSampleIntervalMs) {
    return;
  }
  lastSampleMs = now;

  sensors_event_t humidity;
  sensors_event_t temp;
  if (aht.getEvent(&humidity, &temp)) {
    history.push(temp.temperature, humidity.relative_humidity, true);
    liveValid = true;
    liveTemperature = temp.temperature;
    liveHumidity = humidity.relative_humidity;
    const time_t hour = currentHourStart();
    if (hour > 0 && hourHistory.add(temp.temperature, humidity.relative_humidity, hour)) {
      hourSaveSoon = true;
    }
    Serial.printf("Temperature: %.2f C, Humidity: %.1f%%\n", temp.temperature,
                  humidity.relative_humidity);
  } else {
    history.push(0, 0, false);
    Serial.println("Failed to read AHT20");
  }
}

void handleWifiEvent(arduino_event_id_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    gotIpEpoch = wifiEpoch;
  }
}

void loadCredentials() {
  savedSsid[0] = '\0';
  savedPass[0] = '\0';
  hasSavedCreds = false;

  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, true)) {
    Serial.println("NVS open failed");
    return;
  }

  const bool hasSsid = prefs.isKey("ssid");
  size_t ssidLen = 0;
  size_t passLen = 0;
  if (hasSsid) {
    ssidLen = prefs.getString("ssid", savedSsid, sizeof(savedSsid));
    if (prefs.isKey("pass")) {
      passLen = prefs.getString("pass", savedPass, sizeof(savedPass));
    }
  }
  prefs.end();

  if (!hasSsid || ssidLen == 0 || savedSsid[0] == '\0') {
    savedSsid[0] = '\0';
    savedPass[0] = '\0';
    return;
  }
  if (passLen == 0) {
    savedPass[0] = '\0';
  }
  hasSavedCreds = true;
}

bool saveCredentials() {
  Preferences prefs;
  if (!prefs.begin(kNvsNamespace, false)) {
    Serial.println("NVS open failed");
    return false;
  }

  const size_t ssidStored = prefs.putString("ssid", savedSsid);
  const size_t passStored = prefs.putString("pass", savedPass);
  prefs.end();
  if (ssidStored == 0 || passStored == 0) {
    Serial.println("NVS write failed");
    return false;
  }
  return true;
}

void keepWifiModemSleep() {
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
}

void ensureServices() {
  keepWifiModemSleep();

  const String ip = WiFi.localIP().toString();
  Serial.print("WiFi connected, IP: ");
  Serial.println(ip);
  Serial.printf("Open http://%s/ or http://%s.local/\n", ip.c_str(), kHostname);

  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
  if (MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
  } else {
    Serial.println("mDNS start failed");
  }

  if (!serverStarted) {
    server.begin();
    serverStarted = true;
    Serial.println("HTTP server started");
  }
}

void beginSavedNow() {
  savedBeginPending = false;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.setAutoReconnect(true);
  keepWifiModemSleep();
  WiFi.begin(savedSsid, savedPass);
  lastWifiAttemptMs = millis();
  Serial.printf("Connecting to %s\n", savedSsid);
}

void onSavedWifiConnected() {
  keepWifiModemSleep();
  if (mode == RunMode::ConnectingSaved) {
    mode = RunMode::Streaming;
  }
  ensureServices();
}

void maintainSavedWifi() {
  if (savedBeginPending) {
    if (millis() - savedBeginRequestMs < kProvisionArmDelayMs) {
      return;
    }
    beginSavedNow();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      onSavedWifiConnected();
    }
    return;
  }

  if (wifiWasConnected) {
    Serial.println("WiFi disconnected");
    wifiWasConnected = false;
  }

  const uint32_t now = millis();
  if (now - lastWifiAttemptMs < kWifiRetryMs) {
    return;
  }
  lastWifiAttemptMs = now;
  Serial.println("Reconnecting to WiFi...");
  WiFi.reconnect();
}

bool shouldMaintainSavedWifi() {
  if (!hasSavedCreds || wifiSuspended) {
    return false;
  }
  return mode == RunMode::ConnectingSaved || mode == RunMode::Streaming ||
         mode == RunMode::BtListening;
}

enum class BtAction : uint8_t { Controller, Listen };

struct BtOp {
  SemaphoreHandle_t done;
  BtAction action;
  bool ok;
};

void btOpTask(void* arg) {
  BtOp* op = static_cast<BtOp*>(arg);
  if (op->action == BtAction::Controller) {
    op->ok = btStart();
  } else {
    op->ok = serialBt.begin(kBtName);
  }
  xSemaphoreGive(op->done);
  vTaskDelete(nullptr);
}

bool runBtOp(BtAction action) {
  BtOp op = {};
  op.action = action;
  op.done = xSemaphoreCreateBinary();
  if (op.done == nullptr) {
    Serial.println("Bluetooth task semaphore failed");
    return false;
  }

  const BaseType_t created = xTaskCreatePinnedToCore(
      btOpTask, "btOp", kBtTaskStackBytes, &op, 1, nullptr, 0);
  if (created != pdPASS) {
    vSemaphoreDelete(op.done);
    Serial.println("Bluetooth task start failed");
    return false;
  }

  xSemaphoreTake(op.done, portMAX_DELAY);
  vSemaphoreDelete(op.done);
  return op.ok;
}

void stopBluetooth() {
  if (!btOn) {
    return;
  }
  if (serialBt.hasClient()) {
    serialBt.disconnect();
  }
  if (btStackReady) {
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
  }
  btOn = false;
  btStopRequested = false;
  btLineLen = 0;
  btLineOverflow = false;
  Serial.println("Bluetooth off");
}

void startBluetooth() {
  if (btOn) {
    return;
  }
  btLineLen = 0;
  btLineOverflow = false;
  btStopRequested = false;
  if (!btStackReady) {
    if (!runBtOp(BtAction::Listen)) {
      Serial.println("Bluetooth start failed");
      return;
    }
    btStackReady = true;
  } else {
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  }
  btOn = true;
  Serial.println("Bluetooth on: Terrarium");
}

void serviceBluetoothStop() {
  if (!btStopRequested || !btOn) {
    return;
  }
  if (mode != RunMode::Streaming) {
    btStopRequested = false;
    return;
  }
  if (serialBt.hasClient() && millis() - btStopRequestMs < kBtStopDelayMs) {
    return;
  }
  stopBluetooth();
}

void clearPendingCredentials() {
  memset(pendingSsid, 0, sizeof(pendingSsid));
  memset(pendingPass, 0, sizeof(pendingPass));
}

void finishProvisioningSuccess() {
  memcpy(savedSsid, pendingSsid, sizeof(savedSsid));
  memcpy(savedPass, pendingPass, sizeof(savedPass));
  clearPendingCredentials();
  hasSavedCreds = true;
  wifiSuspended = false;
  savedBeginPending = false;
  provisionArmed = false;

  if (!saveCredentials()) {
    Serial.println("Could not store WiFi credentials");
  }

  keepWifiModemSleep();
  WiFi.setAutoReconnect(true);
  wifiWasConnected = true;
  lastWifiAttemptMs = millis();

  if (btOn) {
    serialBt.print("OK ");
    serialBt.println(WiFi.localIP());
    btStopRequested = true;
    btStopRequestMs = millis();
  }

  mode = RunMode::Streaming;
  ensureServices();
}

void finishProvisioningTimeout() {
  if (btOn) {
    serialBt.println("timeout");
  }
  clearPendingCredentials();
  provisionArmed = false;
  wifiWasConnected = false;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  mode = RunMode::BtListening;
  Serial.println("WiFi connect timeout");

  if (hasSavedCreds) {
    wifiSuspended = false;
    savedBeginPending = true;
    savedBeginRequestMs = millis();
    return;
  }

  wifiSuspended = true;
  savedBeginPending = false;
  WiFi.mode(WIFI_OFF);
}

void armProvisioningBegin() {
  ++wifiEpoch;
  if (wifiEpoch == 0) {
    wifiEpoch = 1;
  }
  provisionEpoch = wifiEpoch;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.setAutoReconnect(false);
  keepWifiModemSleep();
  WiFi.begin(pendingSsid, pendingPass);
  provisionArmed = true;
  lastWifiAttemptMs = millis();
  Serial.printf("Connecting to %s\n", pendingSsid);
}

void startProvisioning(const char* ssid, const char* pass) {
  memset(pendingSsid, 0, sizeof(pendingSsid));
  memset(pendingPass, 0, sizeof(pendingPass));
  memcpy(pendingSsid, ssid, strlen(ssid) + 1);
  memcpy(pendingPass, pass, strlen(pass) + 1);
  mode = RunMode::WifiProvisioning;
  provisionStartMs = millis();
  provisionArmed = false;
  provisionEpoch = 0;
  wifiWasConnected = false;
  wifiSuspended = true;
  savedBeginPending = false;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true, false);
}

void pollProvisioning() {
  const uint32_t elapsed = millis() - provisionStartMs;
  if (!provisionArmed) {
    if (elapsed >= kProvisionTimeoutMs) {
      finishProvisioningTimeout();
      return;
    }
    if (elapsed < kProvisionArmDelayMs) {
      return;
    }
    armProvisioningBegin();
    return;
  }

  if (gotIpEpoch == provisionEpoch && WiFi.status() == WL_CONNECTED &&
      strcmp(WiFi.SSID().c_str(), pendingSsid) == 0) {
    finishProvisioningSuccess();
    return;
  }
  if (elapsed >= kProvisionTimeoutMs) {
    finishProvisioningTimeout();
  }
}

void rejectCommand() {
  serialBt.println("use: connect <SSID> <PASSWORD>");
}

void trimInPlace(char* text) {
  char* start = text;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != text) {
    memmove(text, start, strlen(start) + 1);
  }

  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) {
    text[--len] = '\0';
  }
}

void handleBtLine(char* line) {
  trimInPlace(line);
  if (strncmp(line, "connect", 7) != 0 ||
      (line[7] != '\0' && line[7] != ' ' && line[7] != '\t')) {
    rejectCommand();
    return;
  }

  char* cursor = line + 7;
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  if (*cursor == '\0') {
    rejectCommand();
    return;
  }

  char* ssid = cursor;
  while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
    ++cursor;
  }
  if (*cursor == '\0') {
    rejectCommand();
    return;
  }
  *cursor = '\0';
  ++cursor;
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }

  const size_t ssidLen = strlen(ssid);
  const size_t passLen = strlen(cursor);
  if (ssidLen == 0 || ssidLen > kSsidMax || passLen == 0 || passLen > kPassMax) {
    rejectCommand();
    return;
  }

  startProvisioning(ssid, cursor);
}

void pollBluetooth() {
  if (!btOn) {
    return;
  }

  while (serialBt.available() > 0) {
    const int raw = serialBt.read();
    if (raw < 0) {
      break;
    }

    const char ch = static_cast<char>(raw);
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      btLine[btLineLen] = '\0';
      if (!btLineOverflow && mode == RunMode::BtListening && btLineLen > 0) {
        handleBtLine(btLine);
      }
      btLineLen = 0;
      btLineOverflow = false;
      continue;
    }
    if (btLineOverflow) {
      continue;
    }
    if (btLineLen + 1 >= sizeof(btLine)) {
      btLineOverflow = true;
      btLineLen = 0;
      continue;
    }
    btLine[btLineLen++] = ch;
  }
}

void onBootPressed() {
  if (mode == RunMode::WifiProvisioning) {
    return;
  }

  if (mode == RunMode::BtListening) {
    if (hasSavedCreds && WiFi.status() == WL_CONNECTED) {
      stopBluetooth();
      mode = RunMode::Streaming;
      Serial.println("Bluetooth setup cancelled");
    }
    return;
  }

  const RunMode previous = mode;
  startBluetooth();
  if (!btOn) {
    return;
  }

  if (previous == RunMode::ConnectingSaved) {
    savedBeginPending = false;
    wifiSuspended = true;
    wifiWasConnected = false;
    WiFi.disconnect(false, false);
    WiFi.setAutoReconnect(false);
  }
  mode = RunMode::BtListening;
}

void pollBoot() {
  const bool levelHigh = digitalRead(kBootPin) == HIGH;
  const uint32_t now = millis();
  if (levelHigh != bootRawHigh) {
    bootRawHigh = levelHigh;
    bootChangeMs = now;
  }
  if (now - bootChangeMs < kBootDebounceMs) {
    return;
  }
  if (levelHigh == bootStableHigh) {
    return;
  }
  bootStableHigh = levelHigh;
  if (!bootStableHigh) {
    onBootPressed();
  }
}

void updateLed() {
  if (mode == RunMode::BtListening) {
    digitalWrite(kLedPin, HIGH);
    return;
  }
  if (mode == RunMode::ConnectingSaved || mode == RunMode::WifiProvisioning) {
    const bool on = ((millis() / kLedBlinkHalfMs) % 2) == 0;
    digitalWrite(kLedPin, on ? HIGH : LOW);
    return;
  }
  digitalWrite(kLedPin, LOW);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kBootPin, INPUT_PULLUP);
  pinMode(kLedPin, OUTPUT);
  digitalWrite(kLedPin, LOW);
  bootRawHigh = digitalRead(kBootPin) == HIGH;
  bootStableHigh = bootRawHigh;
  bootChangeMs = millis();

  if (!aht.begin()) {
    Serial.println("Could not find AHT20. Check wiring");
    while (true) {
      delay(10);
    }
  }
  Serial.println("AHT20 found");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/history", HTTP_GET, handleHistory);
  if (!runBtOp(BtAction::Controller)) {
    Serial.println("Bluetooth controller start failed");
  }
  WiFi.onEvent(handleWifiEvent);
  WiFi.persistent(false);

  loadCredentials();
  if (hourHistory.load(kNvsNamespace)) {
    Serial.println("Hour history restored");
  } else {
    Serial.println("Hour history empty");
  }
  lastHourSaveMs = millis();

  if (hasSavedCreds) {
    beginSavedNow();
    mode = RunMode::ConnectingSaved;
  } else {
    WiFi.mode(WIFI_OFF);
    mode = RunMode::Idle;
    Serial.println("No saved WiFi. Press Boot to configure.");
  }
  updateLed();
}

void loop() {
  pollBoot();
  pollBluetooth();
  serviceBluetoothStop();
  if (mode == RunMode::WifiProvisioning) {
    pollProvisioning();
  } else if (shouldMaintainSavedWifi()) {
    maintainSavedWifi();
  }
  maintainClock();
  sampleSensor();
  persistHourHistory();
  if (serverStarted) {
    server.handleClient();
  }
  updateLed();
}
