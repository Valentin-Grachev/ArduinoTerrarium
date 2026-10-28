#pragma once
#include <WebServer.h>
#include "MeasureStorage.h"
#include "TemperatureHumiditySensor.h"
#include "ClockModule.h"

class ServerHandler {
public:
    ServerHandler(int port, MeasureStorage* storage, TemperatureHumiditySensor* sensor, ClockModule* clock);

    bool start();
    void stop();
    void update();

private:
    void handleRoot();
    void handleClimate();
    void handleHistory();

    WebServer _server;
    MeasureStorage* _storage;
    TemperatureHumiditySensor* _sensor;
    ClockModule* _clock;
    bool _isStarted;
};
