#pragma once
#include "TemperatureHumiditySensor.h"
#include "ClockModule.h"
#include <stdint.h>


class MeasureStorage {
public:
    MeasureStorage(const char* filePath);
    void setAutosave(ClockModule* clockModule, TemperatureHumiditySensor* sensor, uint32_t intervalMs);
    bool save(uint32_t utcTimestamp, float temperature, float humidity);
    bool tryGetAverageTemperature(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& value);
    bool tryGetAverageHumidity(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& value);
    char* getData();
    bool clear();
    void update();

private:
    bool initialize();
    bool tryCreateFileWithHeader();
    bool tryRemoveFileLines(int fromIndex, int toIndex);
    bool tryAddFileLine(uint32_t utcTimestamp, float temperature, float humidity);
    bool tryGetFileLinesCount(int& lines);
    bool tryGetAverage(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& temperature, float& humidity);

    const char* _filePath;
    bool _isInitialized;
    uint32_t _lastSaveTimeMs;
    TemperatureHumiditySensor* _sensor;
    ClockModule* _clockModule;
    uint32_t _autosaveIntervalMs;
};
