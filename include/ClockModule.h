#pragma once
#include <functional>
#include <time.h>

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
