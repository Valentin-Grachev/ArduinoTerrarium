#include "WifiConnection.h"
#include <WiFi.h>
#include <utility>

constexpr uint32_t CONNECTION_TIMEOUT_MS = 5000;

WifiConnection::WifiConnection() {
    _wifiState = WIFI_DISCONNECTED;
    _startConnectionTime = 0;
    _linkLostTime = 0;
    _isLinkLost = false;
}

void WifiConnection::connect(const char* ssid, const char* password) {
    if (_wifiState != WIFI_DISCONNECTED) {
        if (onConnectHandled) onConnectHandled(false);
        return;
    }

    _wifiState = WIFI_CONNECTING;
    _startConnectionTime = millis();
    _isLinkLost = false;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
}

void WifiConnection::disconnect() {
    if (_wifiState == WIFI_DISCONNECTED) return;

    WiFi.disconnect(true);

    if (_wifiState == WIFI_CONNECTING) {
        _wifiState = WIFI_DISCONNECTED;
        if (onConnectHandled) onConnectHandled(false);
        return;
    }

    _wifiState = WIFI_DISCONNECTED;
    _isLinkLost = false;

    if (onDisconnected) onDisconnected();
}

void WifiConnection::update() {
    bool connected = WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);

    if (_wifiState == WIFI_CONNECTING) {
        if (connected) {
            _wifiState = WIFI_CONNECTED;
            _isLinkLost = false;
            if (onConnectHandled) onConnectHandled(true);
            return;
        }

        if (millis() - _startConnectionTime > CONNECTION_TIMEOUT_MS) {
            disconnect();
        }

        return;
    }

    if (_wifiState != WIFI_CONNECTED) return;

    if (connected) {
        _isLinkLost = false;
        return;
    }

    if (!_isLinkLost) {
        _isLinkLost = true;
        _linkLostTime = millis();
        return;
    }

    if (millis() - _linkLostTime > CONNECTION_TIMEOUT_MS) {
        disconnect();
    }
}

String WifiConnection::getIp() const {
    return WiFi.localIP().toString();
}
