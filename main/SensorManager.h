#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "MessageTypes.h"

class SensorManager {
public:
    // Initializes DHT sensors and relay pins
    void begin();

    // Starts FreeRTOS sensor read and relay control tasks
    void startTasks();

    // Thread-safe read of the latest sensor snapshot
    SensorData getLatestData();

    // Access to the shared pack (passed into tasks)
    SensorDataPack& getDataPack() { return _dataPack; }

private:
    SensorDataPack _dataPack;

    // ── FreeRTOS task functions ───────────────────────────────────────────────
    // Static wrappers required because FreeRTOS is C and can't call member fns
    static void taskReadSensorsWrapper(void* pvParameters);
    static void taskControlRelaysWrapper(void* pvParameters);

    void taskReadSensors();
    void taskControlRelays();
};

#endif // SENSOR_MANAGER_H