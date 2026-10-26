#pragma once
#include <stdint.h>
#include <functional>

class Button {
public:
    Button(int pin);
    std::function<void()> onClick;
    void update();

private:
    int _pin;
    bool _lastReading;
    bool _isInitialized;
    bool _stableState;
    bool _wasPressed;
    uint32_t _lastChangeMs;
};