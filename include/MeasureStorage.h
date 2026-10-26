#pragma once
#include <stdint.h>

class MeasureStorage {
public:
    MeasureStorage(const char* filePath);
    bool save(uint32_t utcTimestamp, float temperature, float humidity);
    bool tryGetAverageTemperature(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& value);
    bool tryGetAverageHumidity(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& value);
    char* getData();
    bool clear();

private:
    bool initialize();
    bool tryRemoveFileLines(int fromIndex, int toIndex);
    bool tryAddFileLine(uint32_t utcTimestamp, float temperature, float humidity);
    bool tryGetFileLinesCount(int& lines);
    bool tryGetAverage(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& temperature, float& humidity);

    const char* _filePath;
    bool _isInitialized;
};
