#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "MessageTypes.h"

class MqttManager {
public:
    // Singleton access
    static MqttManager& instance();

    // Call once during init. Does not block on connection — maintain() handles that.
    void begin(const char* broker, uint16_t port, const char* clientId);

    // Publishes a SensorData payload as JSON. Returns false if not connected.
    bool publishSensorData(const SensorData& data);

    // Call from the MQTT task loop — reconnects and calls client.loop()
    void maintain();

    bool isConnected();

    // OTA: returns true and fills `out` if an OTA message arrived on TOPIC_OTA
    // since the last call. Safe to call from another task.            [PATCH]
    bool takeOtaMessage(String& out);

private:
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    MqttManager() : _mqttClient(_wifiClient) {}
    MqttManager(const MqttManager&)            = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    bool reconnect();

    WiFiClient   _wifiClient;
    PubSubClient _mqttClient;

    const char* _broker   = nullptr;
    uint16_t    _port     = 1883;
    const char* _clientId = nullptr;
};

#endif // MQTT_MANAGER_H
