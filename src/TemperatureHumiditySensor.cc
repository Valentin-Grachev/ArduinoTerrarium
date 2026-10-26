#include "TemperatureHumiditySensor.h"
#include "Wire.h"

static constexpr uint32_t UPDATE_VALUES_INTERVAL_MS = 1000;

TemperatureHumiditySensor::TemperatureHumiditySensor(int sdaPin, int sclPin) {
    _sdaPin = sdaPin;
    _sclPin = sclPin;
    _temperature = 0;
    _humidity = 0;
}

bool TemperatureHumiditySensor::enable() {
    _sensor.begin();
    updateValues();
    return _isAvailable;
}

bool TemperatureHumiditySensor::isAvailable() {
    return _isAvailable;
}

void TemperatureHumiditySensor::update() {
    if (millis() - _lastUpdateTimeMs > UPDATE_VALUES_INTERVAL_MS) {
        updateValues();
        _lastUpdateTimeMs = millis();
    }
}

void TemperatureHumiditySensor::updateValues() {
    sensors_event_t humidityEvent;
    sensors_event_t temperatureEvent;

    if (_sensor.getEvent(&humidityEvent, &temperatureEvent)) {
        _temperature = temperatureEvent.temperature;
        _humidity = humidityEvent.relative_humidity;
        _isAvailable = true;
    } else {
        _isAvailable = false;
    }

    _lastUpdateTimeMs = millis();
}


float TemperatureHumiditySensor::getHumidity() const {
    return _humidity;
}

float TemperatureHumiditySensor::getTemperature() const {
    return _temperature;
}