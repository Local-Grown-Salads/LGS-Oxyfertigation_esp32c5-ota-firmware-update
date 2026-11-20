#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

class WifiManager {
public:
    // Blocks until connected or timeout. Returns true if connected.
    bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 15000);

    bool isConnected();

    // Call periodically to auto-reconnect if dropped
    void maintain();
};

#endif // WIFI_MANAGER_H