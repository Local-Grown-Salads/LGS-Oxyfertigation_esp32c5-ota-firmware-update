#include "SensorManager.h"
#include "Config.h"
#include <Arduino.h>
#include <DHT.h>

// ── DHT objects live here, not in Config or main ──────────────────────────────
static DHT dhtLF(DHT_LF_PIN, DHT_TYPE);
static DHT dhtLB(DHT_LB_PIN, DHT_TYPE);
// static DHT dhtRB(DHT_RB_PIN, DHT_TYPE);
// static DHT dhtRF(DHT_RF_PIN, DHT_TYPE);

// ── Relay helper — local to this file ────────────────────────────────────────
static void setRelay(int pin, bool on) {
    digitalWrite(pin, on ? LOW : HIGH);
}

// ── Analog water level helper ─────────────────────────────────────────────────
static int waterLevelPercent(int raw) {
    raw = constrain(raw, 0, 4095);
    return map(raw, 0, 4095, 0, 100);
}

// ── begin() ───────────────────────────────────────────────────────────────────
void SensorManager::begin() {
    // Relay pins
    pinMode(RELAY_LF_FAN,   OUTPUT);
    pinMode(RELAY_LB_FAN,   OUTPUT);
    // pinMode(RELAY_RB_FAN,   OUTPUT);
    // pinMode(RELAY_RF_FAN,   OUTPUT);
    pinMode(RELAY_LF_LIGHT, OUTPUT);
    pinMode(RELAY_LB_LIGHT, OUTPUT);
    // pinMode(RELAY_RB_LIGHT, OUTPUT);
    // pinMode(RELAY_RF_LIGHT, OUTPUT);
    // pinMode(RELAY_PUMP,     OUTPUT);
    // pinMode(RELAY_VALVE,    OUTPUT);

    // DHT sensors
    dhtLF.begin();
    dhtLB.begin();
    // dhtRB.begin();
    // dhtRF.begin();

    // Shared data pack
    _dataPack.mutex = xSemaphoreCreateMutex();

    Serial.println("[Sensors] Initialized.");
}

// ── startTasks() ──────────────────────────────────────────────────────────────
void SensorManager::startTasks() {
    // Pass `this` as the parameter so static wrappers can call member functions
    xTaskCreatePinnedToCore(taskReadSensorsWrapper,  "ReadSensors",   TASK_STACK_SENSOR, this, TASK_PRIORITY_SENSOR, NULL, 0);
    xTaskCreatePinnedToCore(taskControlRelaysWrapper,"ControlRelays", TASK_STACK_RELAY,  this, TASK_PRIORITY_RELAY,  NULL, 0);
    Serial.println("[Sensors] Tasks started.");
}

// ── Thread-safe snapshot read ─────────────────────────────────────────────────
SensorData SensorManager::getLatestData() {
    SensorData snapshot = {};
    if (xSemaphoreTake(_dataPack.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        snapshot = _dataPack.data;
        xSemaphoreGive(_dataPack.mutex);
    }
    return snapshot;
}

// ── Static wrappers ───────────────────────────────────────────────────────────
void SensorManager::taskReadSensorsWrapper(void* pvParameters) {
    static_cast<SensorManager*>(pvParameters)->taskReadSensors();
}

void SensorManager::taskControlRelaysWrapper(void* pvParameters) {
    static_cast<SensorManager*>(pvParameters)->taskControlRelays();
}

// ── Task: Read Sensors ────────────────────────────────────────────────────────
void SensorManager::taskReadSensors() {
    for (;;) {
        SensorData local = {};

        // ── DUMMY DATA (replace with real reads once hardware is confirmed) ──
        local.tempLF = 24.5f + (rand() % 10) * 0.1f;
        local.humLF  = 60.0f + (rand() % 10) * 0.1f;
        local.tempLB = 23.8f + (rand() % 10) * 0.1f;
        local.humLB  = 62.0f + (rand() % 10) * 0.1f;
        local.ldrLF  = 1500 + rand() % 500;
        local.ldrLB  = 1600 + rand() % 500;
        local.waterLevelPct = 45 + rand() % 20;

        // ── Real DHT reads (uncomment when hardware is ready) ─────────────────
        // local.tempLF = dhtLF.readTemperature();
        // local.humLF  = dhtLF.readHumidity();
        // local.tempLB = dhtLB.readTemperature();
        // local.humLB  = dhtLB.readHumidity();
        // local.tempRB = dhtRB.readTemperature();
        // local.humRB  = dhtRB.readHumidity();
        // local.tempRF = dhtRF.readTemperature();
        // local.humRF  = dhtRF.readHumidity();
        // local.ldrLF  = analogRead(LDR_LF_PIN);
        // local.ldrLB  = analogRead(LDR_LB_PIN);
        // local.ldrRB  = analogRead(LDR_RB_PIN);
        // local.ldrRF  = analogRead(LDR_RF_PIN);
        // local.waterLevelPct = waterLevelPercent(analogRead(WATER_LEVEL_PIN));

        if (xSemaphoreTake(_dataPack.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            _dataPack.data = local;
            xSemaphoreGive(_dataPack.mutex);
        }

        Serial.printf("[Sensors] LF: %.1f°C %.1f%%  LB: %.1f°C %.1f%%\n",
            local.tempLF, local.humLF, local.tempLB, local.humLB);
        Serial.printf("[Sensors] LDR LF:%d LB:%d  Water:%d%%\n",
            local.ldrLF, local.ldrLB, local.waterLevelPct);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));
    }
}

// ── Task: Control Relays ──────────────────────────────────────────────────────
void SensorManager::taskControlRelays() {
    for (;;) {
        SensorData local = getLatestData();

        setRelay(RELAY_LF_FAN,   !isnan(local.tempLF) && local.tempLF > TEMP_HIGH_THRESHOLD);
        setRelay(RELAY_LB_FAN,   !isnan(local.tempLB) && local.tempLB > TEMP_HIGH_THRESHOLD);
        // setRelay(RELAY_RB_FAN,   !isnan(local.tempRB) && local.tempRB > TEMP_HIGH_THRESHOLD);
        // setRelay(RELAY_RF_FAN,   !isnan(local.tempRF) && local.tempRF > TEMP_HIGH_THRESHOLD);

        setRelay(RELAY_LF_LIGHT, local.ldrLF < LIGHT_DARK_THRESHOLD);
        setRelay(RELAY_LB_LIGHT, local.ldrLB < LIGHT_DARK_THRESHOLD);
        // setRelay(RELAY_RB_LIGHT, local.ldrRB < LIGHT_DARK_THRESHOLD);
        // setRelay(RELAY_RF_LIGHT, local.ldrRF < LIGHT_DARK_THRESHOLD);

        // bool waterLow = local.waterLevelPct < WATER_LOW_THRESHOLD;
        // setRelay(RELAY_PUMP,  waterLow);
        // setRelay(RELAY_VALVE, waterLow);

        vTaskDelay(pdMS_TO_TICKS(RELAY_CHECK_INTERVAL_MS));
    }
}