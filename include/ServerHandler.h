#pragma once
#include <WebServer.h>
#include "MeasureStorage.h"
#include "TemperatureHumiditySensor.h"

class ServerHandler {
public:
    ServerHandler(int port, MeasureStorage* storage, TemperatureHumiditySensor* sensor);

    bool start();
    void stop();
    void update();

private:
    void handleRoot();
    void handleClimate();

    WebServer _server;
    MeasureStorage* _storage;
    TemperatureHumiditySensor* _sensor;
    bool _isStarted;
};
