#pragma once
#include <Arduino.h>
#include <time.h>
#include <esp_sntp.h>
#include <functional>

class ClockModule {
public:
    ClockModule();
    std::function<void()> onSuccessSynced;
    void sync();
    bool isSynced();
    time_t getUtcTimeSec();
    void update();

private:
    bool _isSynced;
};

ClockModule::ClockModule() {
    _isSynced = false;
}

time_t ClockModule::getUtcTimeSec() {
    return time(nullptr);
}

bool ClockModule::isSynced() {
    return _isSynced;
}

void ClockModule::sync() {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
}

void ClockModule::update() {
    if (!_isSynced && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        _isSynced = true;
        if (onSuccessSynced) onSuccessSynced();
    }
}

