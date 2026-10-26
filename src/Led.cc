#include "Led.h"
#include <Arduino.h>

static constexpr uint32_t BLINK_TIMESTEP_MS = 250;


Led::Led(int pin) {
    _pin = pin;
    _isBlinkMode = false;
    _lastToggleMs = 0;
}

void Led::on() {
    _isBlinkMode = false;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH);
}

void Led::off() {
    _isBlinkMode = false;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void Led::blink() {
    _isBlinkMode = true;
}

bool Led::isOn() const {
    return digitalRead(_pin) == HIGH;
}

void Led::update() {
    if (_isBlinkMode && millis() - _lastToggleMs > BLINK_TIMESTEP_MS) {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, isOn() ? LOW : HIGH);
        _lastToggleMs = millis();
    }
}
