#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

class OtaManager {
public:
    static OtaManager& instance();

    void begin();
    void handleOtaMessage(const String& payload);

    bool isUpdateInProgress() const { return _inProgress; }

    // Applies a verified update: writes it to the OTA partition and reboots.
    // Only call this after downloadAndVerify() has succeeded.
    void applyUpdate();

    // Call this once at startup, after WiFi/MQTT/sensors have had a chance
    // to connect. Runs post-update validation and marks the new firmware
    // valid, or rolls back if checks fail.
    void checkAndConfirmBoot();

private:
    OtaManager() = default;
    OtaManager(const OtaManager&)            = delete;
    OtaManager& operator=(const OtaManager&) = delete;

    bool downloadAndVerify(const String& url, const String& expectedHashHex);

    bool _inProgress = false;
    String _lastUrl;
    String _lastHash;
};

#endif // OTA_MANAGER_H