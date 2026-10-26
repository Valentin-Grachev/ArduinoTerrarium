#include <Arduino.h>
#include "Led.h"
#include "BluetoothCommandListener.h"
#include "TemperatureHumiditySensor.h"

const int SDA_PIN = 21;
const int SCL_PIN = 22;
const char* BLUETOOTH_NAME = "ESP32";

Led led(LED_BUILTIN);
BluetoothCommandListener commandListener(BLUETOOTH_NAME);
TemperatureHumiditySensor sensor(SDA_PIN, SCL_PIN);


void setup() {
    Serial.begin(115200);

    sensor.enable();

    commandListener.enable();
    commandListener.onCommandReceived = [](const String& command) {

        if (command == "led on") {
            led.blink();
        }

        if (command == "led off") {
            led.off();
        }

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
    };

}

void loop() {
    led.update();
    commandListener.update();
    sensor.update();
}
