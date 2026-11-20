#ifndef ZIGBEE_COORDINATOR_H
#define ZIGBEE_COORDINATOR_H

#ifdef __cplusplus
extern "C" {
#endif

// Call once from app_main before starting the zigbee task
void zigbee_coordinator_init(void);

// The two tasks — started from app_main via xTaskCreate
void esp_zb_task(void* pvParameters);
void button_task(void* pvParameters);

#ifdef __cplusplus
}
#endif

#endif // ZIGBEE_COORDINATOR_H