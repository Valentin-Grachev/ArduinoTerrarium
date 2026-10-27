#include <Arduino.h>
#include "Led.h"
#include "BluetoothCommandListener.h"
#include "TemperatureHumiditySensor.h"
#include "MeasureStorage.h"
#include "Button.h"
#include "WifiConnection.h"
#include "ClockModule.h"
#include "ServerHandler.h"

const int SEC_MS = 1000;
const int MIN_MS = 60000;

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const int SERVER_PORT = 80;
const int AUTOSAVE_INTERVAL_MS = 10 * MIN_MS;
const char* BLUETOOTH_NAME = "ESP32";
const char* STORAGE_FILE_PATH = "/measures.csv";


ClockModule clockModule;
Led led(LED_BUILTIN);
BluetoothCommandListener bluetoothListener(BLUETOOTH_NAME);
TemperatureHumiditySensor sensor(SDA_PIN, SCL_PIN);
MeasureStorage storage(STORAGE_FILE_PATH);
Button button(0);
WifiConnection wifi;
ServerHandler serverHandler(SERVER_PORT, &storage, &sensor);


void setup() {
    Serial.begin(115200);

    storage.onSaveHandled = [](bool success) {
        Serial.println(success ? "Storage Write: SUCCESS" : "Storage Write: ERROR");
    };

    clockModule.onSuccessSynced = []() {
        Serial.println("Clock Sync: SUCCESS");
    };
    
    button.onClick = []() {
        bluetoothListener.enable();
        led.blink();
    };

    wifi.onConnectHandled = [](bool success) {
        Serial.println(success ? "WiFi Connect: SUCCESS" : "WiFi Connect: ERROR");
    };

    wifi.onDisconnected = [] {
        Serial.println("WiFi Connect: DISCONNECTED");
    };

    bluetoothListener.onCommandReceived = [](const String& command) {

        if (command == "clock sync") {
            clockModule.sync();
        }

        if (command == "storage read") {
            Serial.printf("%s\n", storage.getData());
        }

        if (command == "storage clear") {
            bool success = storage.clear();
            Serial.printf("Storage Clear: %s\n", success ? "SUCCESS" : "ERROR");
        }

        if (command.startsWith("wifi connect ")) {
            int firstSpace = command.indexOf(' ');
            int secondSpace = command.indexOf(' ', firstSpace + 1);
            int thirdSpace = command.indexOf(' ', secondSpace + 1);

            if (thirdSpace != -1) {
                String ssid = command.substring(secondSpace + 1, thirdSpace);
                String password = command.substring(thirdSpace + 1);

                Serial.println("Connecting to SSID: " + ssid + ", Password: " + password);

                wifi.connect(ssid.c_str(), password.c_str());
            }
        }

        if (command == "wifi disconnect") {
            wifi.disconnect();
        }

    };



    wifi.reconnect();
    clockModule.sync();
    sensor.enable();
    storage.setAutosave(&clockModule, &sensor, AUTOSAVE_INTERVAL_MS);
}

void loop() {
    led.update();
    bluetoothListener.update();
    sensor.update();
    button.update();
    wifi.update();
    clockModule.update();
    storage.update();
}
