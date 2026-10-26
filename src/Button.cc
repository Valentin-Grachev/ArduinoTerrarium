#include "Button.h"
#include <Arduino.h>

static constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;

Button::Button(int pin) : _pin(pin), _lastReading(HIGH), _stableState(HIGH),
    _wasPressed(false), _isInitialized(false), _lastChangeMs(0) { }

void Button::update() {
    if (!_isInitialized) {
        pinMode(_pin, INPUT_PULLUP);
        _isInitialized = true;
    }

    bool reading = digitalRead(_pin);

    if (reading != _lastReading) {
        _lastChangeMs = millis();
        _lastReading = reading;
    }

    if (millis() - _lastChangeMs <= BUTTON_DEBOUNCE_MS || reading == _stableState) {
        return;
    }

    _stableState = reading;

    if (_stableState == LOW) {
        _wasPressed = true;
        return;
    }

    if (_wasPressed && onClick) {
        _wasPressed = false;
        onClick();
    }
}