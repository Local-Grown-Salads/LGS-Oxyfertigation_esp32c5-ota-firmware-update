#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// ── Sensor readings from all zones ───────────────────────────────────────────
// Naming: LF = Left Front, LB = Left Back, RB = Right Back, RF = Right Front
struct SensorData {
    float tempLF,  humLF;
    float tempLB,  humLB;
    // float tempRB,  humRB;   // future zones
    // float tempRF,  humRF;

    int   ldrLF, ldrLB;
    // int   ldrRB, ldrRF;     // future zones

    int   waterLevelPct;
};

// ── Thread-safe wrapper passed between tasks ──────────────────────────────────
struct SensorDataPack {
    SensorData       data;
    SemaphoreHandle_t mutex;
};

#endif // MESSAGE_TYPES_H