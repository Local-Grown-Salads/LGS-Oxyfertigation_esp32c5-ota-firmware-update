/*#include "WifiManager.h"
#include "Config.h"
#include <Arduino.h>

bool WifiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutMs) {
            Serial.println("\n[WiFi] Connection timed out.");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::maintain() {
    if (!isConnected()) {
        Serial.println("[WiFi] Dropped. Reconnecting...");
        WiFi.reconnect();
    }
} */ 

//cd ~/Projects/Sujeeth/Part2_Sujeeth
//cat > main/WiFiManager.cpp <<'EOF'
/*#include "WifiManager.h"
#include "Config.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char* TAG_WIFI = "WifiMgr";

// Logs WHY the connection failed (reason code), which Serial.printf logs never showed.
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "STA disconnected, reason=%d", info.wifi_sta_disconnected.reason);
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        ESP_LOGI(TAG_WIFI, "Got IP");
    }
}

bool WifiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWifiEvent);
    // Same fix that stopped the ESP32-C5 5 GHz connect problem in Gardener.
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
    WiFi.setSleep(false);

    ESP_LOGI(TAG_WIFI, "Connecting to SSID '%s'", ssid);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutMs) {
            ESP_LOGE(TAG_WIFI, "Connection timed out");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG_WIFI, "Connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::maintain() {
    if (!isConnected()) {
        ESP_LOGW(TAG_WIFI, "Dropped. Reconnecting...");
        WiFi.reconnect();
    }
} */
/*
#include "WifiManager.h"
#include "Config.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char* TAG_WIFI = "WifiMgr";

// Logs WHY the connection failed (reason code), which Serial.printf logs never showed.
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "STA disconnected, reason=%d", info.wifi_sta_disconnected.reason);
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        ESP_LOGI(TAG_WIFI, "Got IP");
    }
}

bool WifiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWifiEvent);
    // Same fix that stopped the ESP32-C5 5 GHz connect problem in Gardener.
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
    WiFi.setSleep(false);

    ESP_LOGI(TAG_WIFI, "Connecting to SSID '%s'", ssid);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutMs) {
            ESP_LOGE(TAG_WIFI, "Connection timed out");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG_WIFI, "Connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::maintain() {
    // PATCH: maintain() is called every 100 ms. Calling WiFi.reconnect() that often restarts
    // the join each time, so it never completes (endless reason=4 / "sta is connecting").
    // Give each attempt time, and only retry every 10 s.
    static uint32_t lastTry = 0;
    if (isConnected()) { lastTry = millis(); return; }
    if (millis() - lastTry < 10000) return;
    lastTry = millis();
    ESP_LOGW(TAG_WIFI, "Dropped. Reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
} */ 
/* 
#include "WifiManager.h"
#include "Config.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char* TAG_WIFI = "WifiMgr";

// Logs WHY the connection failed (reason code), which Serial.printf logs never showed.
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "STA disconnected, reason=%d", info.wifi_sta_disconnected.reason);
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        ESP_LOGI(TAG_WIFI, "Got IP");
    }
}

bool WifiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWifiEvent);
    // Same fix that stopped the ESP32-C5 5 GHz connect problem in Gardener.
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
    WiFi.setSleep(false);

    ESP_LOGI(TAG_WIFI, "Connecting to SSID '%s'", ssid);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutMs) {
            ESP_LOGE(TAG_WIFI, "Connection timed out");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG_WIFI, "Connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::maintain() {
    // PATCH: maintain() is called every 100 ms. Calling WiFi.reconnect() that often restarts
    // the join each time, so it never completes (endless reason=4 / "sta is connecting").
    // Give each attempt time, and only retry every 10 s.
    static uint32_t lastTry = 0;
    if (isConnected()) { lastTry = millis(); return; }
    if (millis() - lastTry < 10000) return;
    lastTry = millis();
    ESP_LOGW(TAG_WIFI, "Dropped. Reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
} */ 

#include "WifiManager.h"
#include "Config.h"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_log.h"

static const char* TAG_WIFI = "WifiMgr";

// Logs WHY the connection failed (reason code).
static void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        ESP_LOGW(TAG_WIFI, "STA disconnected, reason=%d", info.wifi_sta_disconnected.reason);
    } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        ESP_LOGI(TAG_WIFI, "Got IP");
    }
}

bool WifiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(onWifiEvent);
    // Same fix that stopped the ESP32-C5 5 GHz connect problem in Gardener.
    esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY);
    WiFi.setSleep(false);

    ESP_LOGI(TAG_WIFI, "Connecting to SSID '%s'", ssid);
    WiFi.begin(ssid, password);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutMs) {
            ESP_LOGE(TAG_WIFI, "Connection timed out");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG_WIFI, "Connected. IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

bool WifiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WifiManager::maintain() {
    // Retry at most every 10 s, and start a complete new join with the credentials.
    static uint32_t lastTry = 0;
    if (isConnected()) { lastTry = millis(); return; }
    if (millis() - lastTry < 10000) return;
    lastTry = millis();
    ESP_LOGW(TAG_WIFI, "Dropped. Retrying join to '%s'...", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}