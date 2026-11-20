#include <Arduino.h>
#include "Config.h"
#include "MessageTypes.h"
#include "WifiManager.h"
#include "MqttManager.h"
#include "SensorManager.h"
#include "ZigbeeCoordinator.h"
#include "OtaManager.h"
#include "nvs_flash.h"
#include "esp_coexist.h"
#include "esp_log.h"

// Arduino's initArduino() would otherwise mark a freshly updated image valid
// before our WiFi + MQTT check runs. Returning true defers that to OtaManager.
extern "C" bool verifyRollbackLater() { return true; }

// ── Module instances ──────────────────────────────────────────────────────────
static WifiManager wifiManager;
static SensorManager sensorManager;
// MqttManager is a singleton — accessed via MqttManager::instance()

// ── MQTT publish task ─────────────────────────────────────────────────────────
// Separated from SensorManager because MQTT is a different concern
static void taskPublishMqtt(void *pvParameters)
{
    SensorManager *sensors = static_cast<SensorManager *>(pvParameters);
    MqttManager &mqtt = MqttManager::instance();
    TickType_t lastPublish = xTaskGetTickCount();

    for (;;)
    {
        mqtt.maintain(); // handles reconnect + client.loop()
        wifiManager.maintain();

        if (xTaskGetTickCount() - lastPublish >= pdMS_TO_TICKS(MQTT_PUBLISH_INTERVAL_MS)) {
            SensorData data = sensors->getLatestData();
            mqtt.publishSensorData(data);
            lastPublish = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ── OTA task (PATCH) ──────────────────────────────────────────────────────────
// Own task with a big stack: HTTPS/TLS + download needs far more than the 8 KB
// MQTT task has, and a long download must not block MQTT keepalive.
static void taskOta(void *pvParameters)
{
    String msg;
    for (;;)
    {
        if (MqttManager::instance().takeOtaMessage(msg)) {
            ESP_LOGI("MAIN", "Handling OTA message");
            OtaManager::instance().handleOtaMessage(msg);   // reboots on success
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ── Entry point ───────────────────────────────────────────────────────────────
extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    initArduino();
    Serial.begin(115200);

    esp_err_t err = esp_coex_wifi_i154_enable();
    if (err != ESP_OK)
    {
        Serial.printf("[Coex] enable failed: %s\n", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 1. WiFi
    wifiManager.connect(WIFI_SSID, WIFI_PASSWORD);

    // 2. MQTT (only configures; the connection is made by the publish task below)
    MqttManager::instance().begin(MQTT_BROKER, MQTT_PORT, MQTT_CLIENT_ID);

    // 2b. OTA: log partition info. (The boot check moved further down — PATCH)
    OtaManager::instance().begin();

    // 3. Sensors + relay tasks
    sensorManager.begin();
    sensorManager.startTasks();

    vTaskDelay(pdMS_TO_TICKS(3000));
    // 4. MQTT publish task
    xTaskCreatePinnedToCore(taskPublishMqtt, "MqttPublish", TASK_STACK_MQTT,
                            &sensorManager, TASK_PRIORITY_MQTT, NULL, 0);

    // 5. OTA task + post-update validation (PATCH)
    xTaskCreate(taskOta, "OtaTask", 16384, NULL, 2, NULL);
    // Must run AFTER the MQTT task exists, otherwise MQTT can never connect and
    // every new firmware would be rolled back. Blocks up to 30 s, only right after an OTA.
    OtaManager::instance().checkAndConfirmBoot();

    // zigbee_coordinator_init();
    vTaskDelay(pdMS_TO_TICKS(6000));

    xTaskCreate(esp_zb_task, "zigbee_task", 8192, NULL, 5, NULL);
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

    Serial.println("[Main] All tasks started.");
}

// ── Not used — FreeRTOS tasks handle everything ───────────────────────────────
void setup() {}
void loop() {}
