// #include "Gardener_Sensors.h"
// #include <Arduino.h>
// #include "Config.h"
// #include "MessageTypes.h"

// DHT dhtLF(DHT_LF_PIN, DHT_TYPE);
// DHT dhtLB(DHT_LB_PIN, DHT_TYPE);
// // DHT dhtRB(DHT_RB_PIN, DHT_TYPE);
// // DHT dhtRF(DHT_RF_PIN, DHT_TYPE);

// void setRelay(int pin, bool on) {
//   digitalWrite(pin, on ? LOW : HIGH);
// }

// int waterLevelPercent(int raw) {
//   raw = constrain(raw, 0, 4095);
//   return map(raw, 0, 4095, 0, 100);
// }

// // ── Task 1: Read Sensors (Core 0) ────────────────────────────────
// void taskReadSensors(void* pvParameters) {
//   SensorDataPack* dataPack = (SensorDataPack*)pvParameters;
//   SensorData* data = &dataPack->data;
//   for (;;) {
//     SensorData local;

//     local.tempLF = dhtLF.readTemperature();
//     local.humLF  = dhtLF.readHumidity();
//     local.tempLB = dhtLB.readTemperature();
//     local.humLB  = dhtLB.readHumidity();
//     // local.tempRB = dhtRB.readTemperature();
//     // local.humRB  = dhtRB.readHumidity();
//     // local.tempRF = dhtRF.readTemperature();
//     // local.humRF  = dhtRF.readHumidity();

//     local.ldrLF = analogRead(LDR_LF_PIN);
//     local.ldrLB = analogRead(LDR_LB_PIN);
//     local.ldrRB = analogRead(LDR_RB_PIN);
//     local.ldrRF = analogRead(LDR_RF_PIN);

//     local.waterLevelPct = waterLevelPercent(analogRead(WATER_LEVEL_PIN));

//     if (xSemaphoreTake(dataPack->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//       *data = local;
//       xSemaphoreGive(dataPack->mutex);
//     }

//     Serial.printf("[Sensors] LF:%.1f°C %.1f%%  LB:%.1f°C %.1f%%\n",
//       local.tempLF, local.humLF,
//       local.tempLB, local.humLB);
//       //local.tempRB, local.humRB,
//       //local.tempRF, local.humRF);
//     Serial.printf("[Sensors] LDR LF:%d LB:%d  Water:%d%%\n",
//       local.ldrLF, local.ldrLB,// local.ldrRB, local.ldrRF,
//       local.waterLevelPct);

//     vTaskDelay(pdMS_TO_TICKS(10000));
//   }
// }

// // ── Task 2: Control Relays (Core 1) ──────────────────────────────
// void taskControlRelays(void* pvParameters) {
//   SensorDataPack* dataPack = (SensorDataPack*)pvParameters;
//   SensorData* data = &dataPack->data;
//   for (;;) {
//     SensorData local;

//     if (xSemaphoreTake(dataPack->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//       local = dataPack->data;
//       xSemaphoreGive(dataPack->mutex);
//     }

//     setRelay(RELAY_LF_FAN, !isnan(local.tempLF) && local.tempLF > TEMP_HIGH_THRESHOLD);
//     setRelay(RELAY_LB_FAN, !isnan(local.tempLB) && local.tempLB > TEMP_HIGH_THRESHOLD);
//     // setRelay(RELAY_RB_FAN, !isnan(local.tempRB) && local.tempRB > TEMP_HIGH_THRESHOLD);
//     // setRelay(RELAY_RF_FAN, !isnan(local.tempRF) && local.tempRF > TEMP_HIGH_THRESHOLD);

//     setRelay(RELAY_LF_LIGHT, local.ldrLF < LIGHT_DARK_THRESHOLD);
//     setRelay(RELAY_LB_LIGHT, local.ldrLB < LIGHT_DARK_THRESHOLD);
//     // setRelay(RELAY_RB_LIGHT, local.ldrRB < LIGHT_DARK_THRESHOLD);
//     // setRelay(RELAY_RF_LIGHT, local.ldrRF < LIGHT_DARK_THRESHOLD);

//     bool waterLow = local.waterLevelPct < WATER_LOW_THRESHOLD;
//     // setRelay(RELAY_PUMP,  waterLow);
//     // setRelay(RELAY_VALVE, waterLow);

//     vTaskDelay(pdMS_TO_TICKS(10000));
//   }
// }