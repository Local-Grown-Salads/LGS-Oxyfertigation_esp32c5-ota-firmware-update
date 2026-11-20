/* #include "MqttManager.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <string.h>
#include "esp_log.h"

// ── OTA mailbox (PATCH) ───────────────────────────────────────────────────────
// The MQTT callback runs inside client.loop() on the publish task. It must return
// quickly, so it only copies the message here. A separate OTA task picks it up.
static const char* OTA_TAG = "MQTT_OTA";
static char          s_otaBuf[1024];
static volatile bool s_otaReady = false;

// ── Singleton ─────────────────────────────────────────────────────────────────
MqttManager &MqttManager::instance()
{
    static MqttManager inst;
    return inst;
}

// ── Init ──────────────────────────────────────────────────────────────────────
void MqttManager::begin(const char *broker, uint16_t port, const char *clientId)
{
    _broker = broker;
    _port = port;
    _clientId = clientId;
    _mqttClient.setServer(_broker, _port);
    _mqttClient.setSocketTimeout(15); // seconds, default is 15 but set explicitly
    _mqttClient.setKeepAlive(60);     // keep connection alive longer

    // PATCH: PubSubClient's default buffer is 256 bytes - an OTA message with a long
    // URL + 64-char hash can exceed that and would be silently dropped.
    if (!_mqttClient.setBufferSize(1024)) {
        ESP_LOGE(OTA_TAG, "setBufferSize(1024) failed (out of memory?)");
    }
    _mqttClient.setCallback(mqttCallback);   // PATCH: was never registered

    Serial.printf("[MQTT] Configured broker: %s:%d\n", _broker, _port);
}

// ── Publish ───────────────────────────────────────────────────────────────────
bool MqttManager::publishSensorData(const SensorData &data)
{
    if (!_mqttClient.connected())
    {
        Serial.println("[MQTT] Not connected, skipping publish.");
        return false;
    }

    // Static buffer — avoids heap allocation in task loop
    static char payload[256];
    StaticJsonDocument<200> doc;

    doc["unit"] = UNIT_ID;
    doc["tempLF"] = data.tempLF;
    doc["humLF"] = data.humLF;
    doc["tempLB"] = data.tempLB;
    doc["humLB"] = data.humLB;
    // doc["tempRB"]  = data.tempRB;   // future zones
    // doc["tempRF"]  = data.tempRF;
    doc["ldrLF"] = data.ldrLF;
    doc["ldrLB"] = data.ldrLB;
    doc["waterPct"] = data.waterLevelPct;

    serializeJson(doc, payload, sizeof(payload));

    bool ok = _mqttClient.publish(TOPIC_PUBLISH, payload);
    if (ok)
    {
        Serial.printf("[MQTT] Published: %s\n", payload);
    }
    else
    {
        Serial.println("[MQTT] Publish failed.");
    }
    return ok;
}

// ── Maintain (call from task loop) ────────────────────────────────────────────
void MqttManager::maintain()
{
    if (!_mqttClient.connected())
    {
        reconnect();
    }
    _mqttClient.loop();
}

bool MqttManager::isConnected()
{
    return _mqttClient.connected();
}

// ── Reconnect ─────────────────────────────────────────────────────────────────
bool MqttManager::reconnect() {
    if (_mqttClient.connected()) return true;

    Serial.printf("[MQTT] Connecting to %s...\n", _broker);
    if (_mqttClient.connect(_clientId)) {
        Serial.println("[MQTT] Connected.");
        ESP_LOGI(OTA_TAG, "MQTT connected");
        _mqttClient.subscribe(TOPIC_SUBSCRIBE);
        _mqttClient.subscribe(TOPIC_OTA);          // PATCH: OTA topic was never subscribed
        ESP_LOGI(OTA_TAG, "Subscribed to %s and %s", TOPIC_SUBSCRIBE, TOPIC_OTA);
        return true;
    }

    Serial.printf("[MQTT] Failed. State: %d\n", _mqttClient.state());
    return false;
    // maintain() will retry on next call — no blocking loop needed
}

// ── Incoming messages (PATCH) ─────────────────────────────────────────────────
void MqttManager::mqttCallback(char* topic, byte* payload, unsigned int length)
{
    if (strcmp(topic, TOPIC_OTA) == 0) {
        if (length >= sizeof(s_otaBuf)) {
            ESP_LOGW(OTA_TAG, "OTA message too long (%u bytes) - ignored", length);
            return;
        }
        if (s_otaReady) {
            ESP_LOGW(OTA_TAG, "Previous OTA message not handled yet - ignored");
            return;
        }
        memcpy(s_otaBuf, payload, length);
        s_otaBuf[length] = '\0';
        s_otaReady = true;
        ESP_LOGI(OTA_TAG, "OTA message received (%u bytes)", length);
    }
    // TOPIC_SUBSCRIBE (commands) is not handled in this project yet.
}

bool MqttManager::takeOtaMessage(String& out)
{
    if (!s_otaReady) return false;
    out = String(s_otaBuf);
    s_otaReady = false;
    return true;
} */ //previously in main/MqttManager.cpp.


#include "MqttManager.h"
#include "Config.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <string.h>
#include "esp_log.h"

// ── OTA mailbox (PATCH) ───────────────────────────────────────────────────────
// The MQTT callback runs inside client.loop() on the publish task. It must return
// quickly, so it only copies the message here. A separate OTA task picks it up.
static const char* OTA_TAG = "MQTT_OTA";
static char          s_otaBuf[1024];
static volatile bool s_otaReady = false;

// ── Singleton ─────────────────────────────────────────────────────────────────
MqttManager &MqttManager::instance()
{
    static MqttManager inst;
    return inst;
}

// ── Init ──────────────────────────────────────────────────────────────────────
void MqttManager::begin(const char *broker, uint16_t port, const char *clientId)
{
    _broker = broker;
    _port = port;
    _clientId = clientId;
    _mqttClient.setServer(_broker, _port);
    _mqttClient.setSocketTimeout(15); // seconds, default is 15 but set explicitly
    _mqttClient.setKeepAlive(60);     // keep connection alive longer

    // PATCH: PubSubClient's default buffer is 256 bytes - an OTA message with a long
    // URL + 64-char hash can exceed that and would be silently dropped.
    if (!_mqttClient.setBufferSize(1024)) {
        ESP_LOGE(OTA_TAG, "setBufferSize(1024) failed (out of memory?)");
    }
    _mqttClient.setCallback(mqttCallback);   // PATCH: was never registered

    Serial.printf("[MQTT] Configured broker: %s:%d\n", _broker, _port);
}

// ── Publish ───────────────────────────────────────────────────────────────────
bool MqttManager::publishSensorData(const SensorData &data)
{
    if (!_mqttClient.connected())
    {
        Serial.println("[MQTT] Not connected, skipping publish.");
        return false;
    }

    // Static buffer — avoids heap allocation in task loop
    static char payload[256];
    StaticJsonDocument<200> doc;

    doc["unit"] = UNIT_ID;
    doc["tempLF"] = data.tempLF;
    doc["humLF"] = data.humLF;
    doc["tempLB"] = data.tempLB;
    doc["humLB"] = data.humLB;
    // doc["tempRB"]  = data.tempRB;   // future zones
    // doc["tempRF"]  = data.tempRF;
    doc["ldrLF"] = data.ldrLF;
    doc["ldrLB"] = data.ldrLB;
    doc["waterPct"] = data.waterLevelPct;

    serializeJson(doc, payload, sizeof(payload));

    bool ok = _mqttClient.publish(TOPIC_PUBLISH, payload);
    if (ok)
    {
        Serial.printf("[MQTT] Published: %s\n", payload);
    }
    else
    {
        Serial.println("[MQTT] Publish failed.");
    }
    return ok;
}

// ── Maintain (call from task loop) ────────────────────────────────────────────
void MqttManager::maintain()
{
    if (!_mqttClient.connected())
    {
        // PATCH: retry at most every 5 s and only with WiFi up (was a 100 ms tight loop)
        static uint32_t lastTry = 0;
        if (WiFi.status() == WL_CONNECTED && millis() - lastTry >= 5000)
        {
            lastTry = millis();
            reconnect();
        }
    }
    _mqttClient.loop();
}

bool MqttManager::isConnected()
{
    return _mqttClient.connected();
}

// ── Reconnect ─────────────────────────────────────────────────────────────────
bool MqttManager::reconnect() {
    if (_mqttClient.connected()) return true;

    Serial.printf("[MQTT] Connecting to %s...\n", _broker);
    if (_mqttClient.connect(_clientId)) {
        Serial.println("[MQTT] Connected.");
        ESP_LOGI(OTA_TAG, "MQTT connected");
        _mqttClient.subscribe(TOPIC_SUBSCRIBE);
        _mqttClient.subscribe(TOPIC_OTA);          // PATCH: OTA topic was never subscribed
        ESP_LOGI(OTA_TAG, "Subscribed to %s and %s", TOPIC_SUBSCRIBE, TOPIC_OTA);
        return true;
    }

    Serial.printf("[MQTT] Failed. State: %d\n", _mqttClient.state());
    return false;
    // maintain() will retry on next call — no blocking loop needed
}

// ── Incoming messages (PATCH) ─────────────────────────────────────────────────
void MqttManager::mqttCallback(char* topic, byte* payload, unsigned int length)
{
    if (strcmp(topic, TOPIC_OTA) == 0) {
        if (length >= sizeof(s_otaBuf)) {
            ESP_LOGW(OTA_TAG, "OTA message too long (%u bytes) - ignored", length);
            return;
        }
        if (s_otaReady) {
            ESP_LOGW(OTA_TAG, "Previous OTA message not handled yet - ignored");
            return;
        }
        memcpy(s_otaBuf, payload, length);
        s_otaBuf[length] = '\0';
        s_otaReady = true;
        ESP_LOGI(OTA_TAG, "OTA message received (%u bytes)", length);
    }
    // TOPIC_SUBSCRIBE (commands) is not handled in this project yet.
}

bool MqttManager::takeOtaMessage(String& out)
{
    if (!s_otaReady) return false;
    out = String(s_otaBuf);
    s_otaReady = false;
    return true;
}