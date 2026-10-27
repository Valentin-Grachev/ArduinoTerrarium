#include "ServerHandler.h"
#include <LittleFS.h>

LocalServer::LocalServer(int port, MeasureStorage& storage, TemperatureHumiditySensor& sensor)
    : _server(port), _storage(storage), _sensor(sensor), _isStarted(false) {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/climate", HTTP_GET, [this]() { handleClimate(); });
}

bool LocalServer::start() {
    if (_isStarted) return true;

    if (!LittleFS.begin(true)) return false;

    _server.begin();
    _isStarted = true;
    return true;
}

void LocalServer::stop() {
    if (!_isStarted) return;

    _server.stop();
    _isStarted = false;
}

void LocalServer::update() {
    if (!_isStarted) return;

    _server.handleClient();
}

void LocalServer::handleRoot() {
    File file = LittleFS.open("/index.html", FILE_READ);
    if (!file) {
        _server.send(404, "text/plain", "index.html not found");
        return;
    }

    _server.streamFile(file, "text/html");
    file.close();
}

void LocalServer::handleClimate() {
    String body = "{\"available\":";
    body += _sensor.isAvailable() ? "true" : "false";
    body += ",\"temperature\":";
    body += String(_sensor.getTemperature(), 2);
    body += ",\"humidity\":";
    body += String(_sensor.getHumidity(), 2);
    body += "}";

    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", body);
}
