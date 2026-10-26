#include "BluetoothCommandListener.h"

BluetoothCommandListener::BluetoothCommandListener(const char* deviceName) : _deviceName(deviceName) {}

void BluetoothCommandListener::enable() {
    _bluetoothSerial.begin(_deviceName);
}

void BluetoothCommandListener::disable() {
    _bluetoothSerial.end();
}

void BluetoothCommandListener::update() {
    while (_bluetoothSerial.available()) {
        char c = _bluetoothSerial.read();

        if (c == '\n') {
            _buffer.trim();

            if (!_buffer.isEmpty() && onCommandReceived) {
                onCommandReceived(_buffer);
            }

            _buffer = "";
        } else {
            _buffer += c;
        }
    }
}