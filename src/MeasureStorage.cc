#include "MeasureStorage.h"
#include <LittleFS.h>
#include <math.h>
#include <stdio.h>

constexpr int MAX_MEASURES = 2000;
constexpr int CLEAR_LINES = 500;


MeasureStorage::MeasureStorage(const char* filePath) { 
    _filePath = filePath;
    _isInitialized = false;
}

bool MeasureStorage::initialize() {
    if (_isInitialized || !LittleFS.begin(false)) 
        return false;

    if (!LittleFS.exists(_filePath)) {
        File file = LittleFS.open(_filePath, FILE_WRITE);
        if (!file) return false;

        file.println("utcTimestamp,temperature,humidity");

        bool success = file.getWriteError() == 0;
        file.close();

        if (!success) return false;
    }

    _isInitialized = true;
    return true;
}


bool MeasureStorage::save(uint32_t utcTimestamp, float temperature, float humidity) {
    if (!_isInitialized && !initialize()) 
        return false; 

    int linesCount = 0;
    if (!tryGetFileLinesCount(linesCount))
        return false;

    if (linesCount < MAX_MEASURES) 
        return tryAddFileLine(utcTimestamp, temperature, humidity);

    else { // Escess capacity

        if (!tryRemoveFileLines(1, CLEAR_LINES))
            return false;

        return tryAddFileLine(utcTimestamp, temperature, humidity);
    }
}

bool MeasureStorage::tryGetAverageHumidity(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& value) {
    float t;
    return tryGetAverage(fromUtcTimestamp, toUtcTimestamp, t, value);
}

bool MeasureStorage::tryGetAverageTemperature(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& value) {
    float h;
    return tryGetAverage(fromUtcTimestamp, toUtcTimestamp, value, h);
}

bool MeasureStorage::tryGetAverage(uint32_t fromUtcTimestamp, uint32_t toUtcTimestamp, float& temperature, float& humidity) {
    temperature = 0;
    humidity = 0;
    
    if (!_isInitialized && !initialize() || fromUtcTimestamp > toUtcTimestamp)
        return false;

    File file = LittleFS.open(_filePath, FILE_READ);
    if (!file) return false;

    // Skip header
    file.readStringUntil('\n');

    double sumT = 0.0, sumH = 0.0;
    uint32_t count = 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.length() == 0) continue;

        unsigned long timestamp;
        float t, h;

        if (sscanf(line.c_str(), "%lu,%f,%f", &timestamp, &t, &h) != 3)
            continue;

        if (timestamp < fromUtcTimestamp || timestamp > toUtcTimestamp) 
            continue;

        sumT += t;
        sumH += h;

        count++;
    }

    file.close();

    if (count == 0) return false;

    temperature = sumT / count;
    humidity = sumH / count;

    return true;
}


bool MeasureStorage::tryRemoveFileLines(int fromIndex, int toIndex) {
    if (fromIndex < 0 || toIndex < fromIndex) 
        return false;

    File source = LittleFS.open(_filePath, FILE_READ);
    if (!source) return false;

    File temp = LittleFS.open("/temp.csv", FILE_WRITE);
    if (!temp) {
        source.close();
        return false;
    }

    int index = 0;

    while (source.available()) {
        String line = source.readStringUntil('\n');

        if (index < fromIndex || index > toIndex) {
            if (temp.println(line) == 0) {
                source.close();
                temp.close();
                LittleFS.remove("/temp.csv");
                return false;
            }
        }

        index++;
    }

    source.close();
    temp.close();

    if (!LittleFS.remove(_filePath)) {
        LittleFS.remove("/temp.csv");
        return false;
    }

    return LittleFS.rename("/temp.csv", _filePath);
}

bool MeasureStorage::tryAddFileLine(uint32_t utcTimestamp, float temperature, float humidity) {
    File file = LittleFS.open(_filePath, FILE_APPEND);
    if (!file) return false;
        
    file.printf("%lu,%.2f,%.2f\n", 
        static_cast<unsigned long>(utcTimestamp), temperature, humidity);

    bool success = file.getWriteError() == 0;
    file.close();

    return success;
}

bool MeasureStorage::tryGetFileLinesCount(int& lines) {
    lines = 0;

    File file = LittleFS.open(_filePath, FILE_READ);
    if (!file) return false;

    file.readStringUntil('\n');

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.length() > 0) lines++;
    }

    file.close();

    return true;
}

bool MeasureStorage::clear() {

}

char* MeasureStorage::getData() {

}