#pragma once
#include <Adafruit_AHTX0.h>


class TemperatureHumiditySensor
{
public:
    TemperatureHumiditySensor(int sdaPin, int sclPin);
    bool enable();
    bool isAvailable();
    void update();
    float getTemperature() const;
    float getHumidity() const;

private:
    void updateValues();

    int _sdaPin, _sclPin;
    Adafruit_AHTX0 _sensor;
    float _humidity;
    float _temperature;
    bool _isAvailable;
    uint32_t _lastUpdateTimeMs;
};

