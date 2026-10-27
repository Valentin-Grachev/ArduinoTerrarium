#pragma once
#include <Arduino.h>
#include <functional>
#include <Preferences.h>

constexpr int WIFI_DISCONNECTED = 0;
constexpr int WIFI_CONNECTING = 1;
constexpr int WIFI_CONNECTED = 2;

class WifiConnection {
public:
    WifiConnection();
    std::function<void(bool)> onConnectHandled;
    std::function<void()> onDisconnected;
    void connect(const char* ssid, const char* password);
    void reconnect();
    void disconnect();
    void update();
    String getIp() const;

private:
    bool openPreferences();

    int _wifiState;
    uint32_t _startConnectionTime;
    uint32_t _linkLostTime;
    bool _isLinkLost;
    bool _preferencesOpen;
    Preferences _preferences;
    String _ssid;
    String _password;
};

