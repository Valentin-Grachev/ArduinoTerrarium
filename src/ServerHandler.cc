#include "ServerHandler.h"
#include <LittleFS.h>

ServerHandler::ServerHandler(int port, MeasureStorage* storage, TemperatureHumiditySensor* sensor, ClockModule* clock)
    : _server(port), _storage(storage), _sensor(sensor), _clock(clock), _isStarted(false) {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/climate", HTTP_GET, [this]() { handleClimate(); });
    _server.on("/api/history", HTTP_GET, [this]() { handleHistory(); });
}

bool ServerHandler::start() {
    if (_isStarted) return true;

    if (!LittleFS.begin(true)) return false;

    _server.begin();
    _isStarted = true;
    return true;
}

void ServerHandler::stop() {
    if (!_isStarted) return;

    _server.stop();
    _isStarted = false;
}

void ServerHandler::update() {
    if (!_isStarted) return;

    _server.handleClient();
}

void ServerHandler::handleRoot() {
    File file = LittleFS.open("/index.html", FILE_READ);
    if (!file) {
        _server.send(404, "text/plain", "index.html not found");
        return;
    }

    _server.streamFile(file, "text/html");
    file.close();
}

void ServerHandler::handleClimate() {
    String body = "{\"available\":";
    body += _sensor->isAvailable() ? "true" : "false";
    body += ",\"temperature\":";
    body += String(_sensor->getTemperature(), 2);
    body += ",\"humidity\":";
    body += String(_sensor->getHumidity(), 2);
    body += "}";

    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", body);
}

void ServerHandler::handleHistory() {
    if (!_clock->isSynced()) {
        _server.sendHeader("Cache-Control", "no-store");
        _server.send(200, "application/json", "{\"synced\":false,\"hours\":[]}");
        return;
    }

    uint32_t nowSec = static_cast<uint32_t>(_clock->getUtcTimeSec());
    uint32_t currentHour = nowSec - (nowSec % 3600);
    uint32_t firstHour = currentHour - 23u * 3600u;

    String body = "{\"synced\":true,\"hours\":[";
    for (int i = 0; i < 24; i++) {
        uint32_t from = firstHour + static_cast<uint32_t>(i) * 3600u;
        uint32_t to = from + 3599u;

        float temperature = 0;
        float humidity = 0;
        bool hasTemperature = _storage->tryGetAverageTemperature(from, to, temperature);
        bool hasHumidity = _storage->tryGetAverageHumidity(from, to, humidity);

        if (i > 0) body += ",";
        body += "{\"from\":";
        body += String(from);
        body += ",\"temperature\":";
        body += hasTemperature ? String(temperature, 1) : String("null");
        body += ",\"humidity\":";
        body += hasHumidity ? String(humidity, 1) : String("null");
        body += "}";
    }
    body += "]}";

    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json", body);
}
