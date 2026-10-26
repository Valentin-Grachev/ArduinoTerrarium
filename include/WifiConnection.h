#pragma once
#include <Arduino.h>
#include <functional>

constexpr int WIFI_DISCONNECTED = 0;
constexpr int WIFI_CONNECTING = 1;
constexpr int WIFI_CONNECTED = 2;

class WifiConnection {
public:
    WifiConnection();
    std::function<void()> onDisconnected;
    void connect(const char* ssid, const char* password, std::function<void(bool)> onHandled);
    void disconnect();
    void update();
    String getIp() const;

private:
    int _wifiState;
    uint32_t _startConnectionTime;
    std::function<void(bool)> _onConnectHandled;
};

