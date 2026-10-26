#pragma once
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <functional>

class BluetoothCommandListener {
public:
    BluetoothCommandListener(const char* deviceName);
    std::function<void(const String&)> onCommandReceived;
    void enable();
    void disable();
    void update();

private:
    BluetoothSerial _bluetoothSerial;
    String _buffer;
    const char* _deviceName;
};