#include <Arduino.h>
#include "Led.h"
#include "BluetoothCommandListener.h"
#include "TemperatureHumiditySensor.h"
#include "MeasureStorage.h"
#include "Button.h"
#include "WifiConnection.h"
#include <Preferences.h>


const int SDA_PIN = 21;
const int SCL_PIN = 22;
const char* BLUETOOTH_NAME = "ESP32";
const char* STORAGE_FILE_PATH = "/measures.csv";

Led led(LED_BUILTIN);
BluetoothCommandListener bluetoothListener(BLUETOOTH_NAME);
TemperatureHumiditySensor sensor(SDA_PIN, SCL_PIN);
MeasureStorage storage(STORAGE_FILE_PATH);
Button button(0);
WifiConnection wifi;
Preferences preferences;


void setup() {
    Serial.begin(115200);

    sensor.enable();

    button.onClick = []() {
        bluetoothListener.enable();
        led.blink();
    };

    wifi.onConnectHandled = [](bool success) {
        Serial.println(success ? "WiFi connect: SUCCESS" : "WiFi connect: ERROR");
    };

    wifi.onDisconnected = [] {
        Serial.println("WiFi disconnected");
    };

    bluetoothListener.onCommandReceived = [](const String& command) {

        if (command == "sensor t") {
            if (sensor.isAvailable()) {
                Serial.println(sensor.getTemperature());
            } else {
                Serial.println("Sensor is not available");
            }
        }

        if (command == "sensor h") {
            if (sensor.isAvailable()) {
                Serial.println(sensor.getHumidity());
            } else {
                Serial.println("Sensor is not available");
            }
        }

        if (command == "storage save") {
            bool success = storage.save(0, sensor.getHumidity(), sensor.getTemperature());
            Serial.printf("Save status: %s\n", success ? "SUCCESS" : "ERROR");
        }

        if (command == "storage read") {
            Serial.printf("%s\n", storage.getData());
        }

        if (command == "storage clear") {
            bool success = storage.clear();
            Serial.printf("Clear status: %s\n", success ? "SUCCESS" : "ERROR");
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

}

void loop() {
    led.update();
    bluetoothListener.update();
    sensor.update();
    button.update();
    wifi.update();
}
