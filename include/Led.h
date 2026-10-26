#pragma once
#include <stdint.h>

class Led {
public:
    Led(int pin);
    void on();
    void off();
    void blink();
    void update();
    bool isOn() const;

private:
    int _pin;
    bool _isBlinkMode;
    uint32_t _lastToggleMs;
};
