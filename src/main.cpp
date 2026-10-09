#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "SensorHistory.h"
#include "index_html.h"
#include "secrets.h"

namespace {

constexpr uint32_t kSampleIntervalMs = 1000;
constexpr uint32_t kWifiRetryMs = 10000;
constexpr char kHostname[] = "terrarium";

Adafruit_AHTX0 aht;
WebServer server(80);
SensorHistory history;

char jsonBuf[2048];
uint32_t lastSampleMs = 0;
uint32_t lastWifiAttemptMs = 0;
bool wifiWasConnected = false;

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

const char* buildHistoryJson() {
  char* cursor = jsonBuf;
  char* end = jsonBuf + sizeof(jsonBuf);
  if (!appendFmt(cursor, end, "{\"interval_s\":1,\"capacity\":%u,\"temperature\":[",
                 static_cast<unsigned>(SensorHistory::kCapacity)) ||
      !appendSeries(cursor, end, true) ||
      !appendFmt(cursor, end, "],\"humidity\":[") ||
      !appendSeries(cursor, end, false) ||
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
    Serial.printf("Temperature: %.2f C, Humidity: %.1f%%\n", temp.temperature,
                  humidity.relative_humidity);
  } else {
    history.push(0, 0, false);
    Serial.println("Failed to read AHT20");
  }
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiWasConnected = true;
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

void connectWifi() {
  if (strcmp(WIFI_SSID, "your-ssid") == 0 || strcmp(WIFI_PASSWORD, "your-password") == 0) {
    Serial.println("Set WIFI_SSID and WIFI_PASSWORD in include/secrets.h");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("Connecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  WiFi.setSleep(false);

  const String ip = WiFi.localIP().toString();
  Serial.print("WiFi connected, IP: ");
  Serial.println(ip);
  Serial.printf("Open http://%s/ or http://%s.local/\n", ip.c_str(), kHostname);

  if (MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("mDNS start failed");
  }

  wifiWasConnected = true;
  lastWifiAttemptMs = millis();
}

}  // namespace

void setup() {
  Serial.begin(115200);

  if (!aht.begin()) {
    Serial.println("Could not find AHT20. Check wiring");
    while (true) {
      delay(10);
    }
  }
  Serial.println("AHT20 found");

  connectWifi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  sampleSensor();
  server.handleClient();
  maintainWifi();
}
