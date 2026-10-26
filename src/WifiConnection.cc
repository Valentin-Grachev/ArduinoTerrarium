#include "WifiConnection.h"
#include <WiFi.h>

constexpr uint32_t CONNECTION_TIMEOUT_MS = 10000;


WifiConnection::WifiConnection() {
    _wifiState = WIFI_DISCONNECTED;
}

void WifiConnection::connect(const char* ssid, const char* password, std::function<void(bool)> onHandled) {
    if (_wifiState != WIFI_DISCONNECTED) {
        if (onHandled) onHandled(false);
        return;
    }

    _wifiState = WIFI_CONNECTING;
    _onConnectHandled = onHandled;
    _startConnectionTime = millis();

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
}

void WifiConnection::disconnect() {
    if (_wifiState == WIFI_DISCONNECTED) return;

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    _wifiState = WIFI_DISCONNECTED;
    if (onDisconnected) onDisconnected();
}

void WifiConnection::update() {
    bool isConnected = WiFi.status() == WL_CONNECTED;

    if (_wifiState == WIFI_CONNECTING) {
        if (isConnected) {
            _wifiState = WIFI_CONNECTED;
            if (_onConnectHandled) _onConnectHandled(true);
            return;
        }
        else if (millis() - _startConnectionTime > CONNECTION_TIMEOUT_MS) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            _wifiState = WIFI_DISCONNECTED;
            if (_onConnectHandled) _onConnectHandled(false);
        }
    }

    if (!isConnected && _wifiState == WIFI_CONNECTED) {
        disconnect();
        return;
    }
}


String WifiConnection::getIp() const {
    return WiFi.localIP().toString();
}

