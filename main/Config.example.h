#ifndef CONFIG_H
#define CONFIG_H

// ── Unit Identity ─────────────────────────────────────────────────────────────
#define UNIT_ID         1

// ── WiFi ──────────────────────────────────────────────────────────────────────
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ── MQTT ──────────────────────────────────────────────────────────────────────
#define MQTT_BROKER "YOUR_BROKER_IP"
#define MQTT_PORT       1883
#define MQTT_CLIENT_ID  "FarmESP32C5_1"
#define TOPIC_PUBLISH   "GardenerWaterTemperatureSensorData"
#define TOPIC_SUBSCRIBE "topic_cmd_1"
#define TOPIC_OTA       "topic_ota_1"

// ── DHT Sensor Pins ───────────────────────────────────────────────────────────
#define DHT_LF_PIN      23
#define DHT_LB_PIN       4
// #define DHT_RB_PIN    21
// #define DHT_RF_PIN    22
#define DHT_TYPE        DHT22

// ── LDR Pins ──────────────────────────────────────────────────────────────────
#define LDR_LF_PIN      0
#define LDR_LB_PIN      1
#define LDR_RB_PIN      2
#define LDR_RF_PIN      3

// ── Water Level ───────────────────────────────────────────────────────────────
#define WATER_LEVEL_PIN 6

// ── Relay Pins ────────────────────────────────────────────────────────────────
#define RELAY_LF_LIGHT  5
#define RELAY_LF_FAN    7
#define RELAY_LB_LIGHT  8
#define RELAY_LB_FAN    9
// #define RELAY_RB_LIGHT  10
// #define RELAY_RB_FAN    11
// #define RELAY_RF_LIGHT  12
// #define RELAY_RF_FAN    13
// #define RELAY_PUMP      18
// #define RELAY_VALVE     19

// ── Thresholds ────────────────────────────────────────────────────────────────
#define TEMP_HIGH_THRESHOLD     30.0f
#define LIGHT_DARK_THRESHOLD    1800
#define WATER_LOW_THRESHOLD     20

// ── FreeRTOS Task Config ──────────────────────────────────────────────────────
#define TASK_STACK_SENSOR       4096
#define TASK_STACK_RELAY        4096
#define TASK_STACK_MQTT         8192    // Arduino libs need more stack
#define TASK_PRIORITY_SENSOR    1
#define TASK_PRIORITY_RELAY     1
#define TASK_PRIORITY_MQTT      2

#define SENSOR_READ_INTERVAL_MS     10000
#define RELAY_CHECK_INTERVAL_MS     10000
#define MQTT_PUBLISH_INTERVAL_MS    15000

#endif // CONFIG_H