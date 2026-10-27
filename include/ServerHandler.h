#pragma once
#include <WebServer.h>
#include "MeasureStorage.h"
#include "TemperatureHumiditySensor.h"

class LocalServer {
public:
    LocalServer(int port, MeasureStorage& storage, TemperatureHumiditySensor& sensor);

    bool start();
    void stop();
    void update();

private:
    void handleRoot();
    void handleClimate();

    WebServer _server;
    MeasureStorage& _storage;
    TemperatureHumiditySensor& _sensor;
    bool _isStarted;
};
