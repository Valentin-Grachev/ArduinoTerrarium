#include "ClockModule.h"
#include <Arduino.h>
#include <esp_sntp.h>

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
