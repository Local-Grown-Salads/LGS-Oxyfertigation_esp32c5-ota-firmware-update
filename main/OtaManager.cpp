// OtaManager.cpp  -  Project 2 (ref Sujeeth)  -  PATCHED version of Sujeeth's file.
// OtaManager.h is UNCHANGED.
//
// What changed vs. Sujeeth's original:
//   1. Logging uses ESP_LOG (Serial.printf does not show in idf.py monitor on our setup).
//   2. Accepts plain http:// (LAN testing) as well as https://. Original forced TLS.
//   3. Follows HTTP redirects (GitHub release links answer 302).
//   4. Downloads ONCE: downloadAndVerify() streams into the inactive OTA slot while
//      hashing; applyUpdate() only commits (esp_ota_end + set boot partition + reboot).
//      Original downloaded the file twice.
//   5. Checks the firmware fits the slot, requires Content-Length, detects stalls.
//   6. checkAndConfirmBoot() WAITS (up to 30 s) for WiFi + MQTT instead of testing
//      once. Call it AFTER the MQTT task has been started (see main.cpp).
//   7. Message accepts "hash" (Sujeeth's key) or "sha256".
//
// OTA message (MQTT, topic TOPIC_OTA from Config.h):
//   {"url":"http://<mac-ip>:8000/firmware.bin","hash":"<64 hex chars, SHA-256>"}

#include "OtaManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "MqttManager.h"

static const char* TAG = "OtaManager";

// Change this to "v2", "v3" ... before building the update images so you can tell
// which firmware is running from the boot log.
#ifndef FW_TAG
#define FW_TAG "v5"
#endif

// ---- Testing shortcuts. Set both to 0 for production (Stage 10). -----------
#ifndef OTA_ALLOW_HTTP
#define OTA_ALLOW_HTTP 1            // allow plain http:// URLs
#endif
#ifndef OTA_ALLOW_INSECURE_TLS
#define OTA_ALLOW_INSECURE_TLS 1    // https without certificate check
#endif

#ifndef OTA_BOOT_CHECK_TIMEOUT_MS
#define OTA_BOOT_CHECK_TIMEOUT_MS 30000
#endif
#ifndef OTA_STALL_TIMEOUT_MS
#define OTA_STALL_TIMEOUT_MS 15000
#endif

// State carried from downloadAndVerify() to applyUpdate()
static esp_ota_handle_t       s_handle = 0;
static const esp_partition_t* s_target = nullptr;
static bool                   s_ready  = false;   // fully written + hash verified

static String toHex(const uint8_t* b, size_t n) {
    static const char* H = "0123456789abcdef";
    String s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s += H[b[i] >> 4]; s += H[b[i] & 0x0F]; }
    return s;
}

// ---------------------------------------------------------------------------
OtaManager& OtaManager::instance() {
    static OtaManager inst;
    return inst;
}

// ---------------------------------------------------------------------------
void OtaManager::begin() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* next    = esp_ota_get_next_update_partition(nullptr);
    ESP_LOGI(TAG, "FIRMWARE TAG: %s", FW_TAG);
    if (running) {
        esp_app_desc_t desc;
        if (esp_ota_get_partition_description(running, &desc) == ESP_OK) {
            ESP_LOGI(TAG, "Built: %s %s", desc.date, desc.time);
        }
        ESP_LOGI(TAG, "Running partition: %s @0x%x", running->label, (unsigned)running->address);
    }
    if (next) {
        ESP_LOGI(TAG, "Next OTA slot:     %s @0x%x", next->label, (unsigned)next->address);
    } else {
        ESP_LOGE(TAG, "No OTA slot found - check partitions.csv (otadata + ota_0 + ota_1)");
    }
}

// ---------------------------------------------------------------------------
void OtaManager::handleOtaMessage(const String& payload) {
    if (_inProgress) {
        ESP_LOGW(TAG, "Update already in progress, ignoring new OTA message");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        ESP_LOGE(TAG, "Failed to parse OTA message: %s", err.c_str());
        return;
    }

    const char* url  = doc["url"];
    const char* hash = doc["hash"];
    if (!hash) hash = doc["sha256"];
    if (!url || !hash) {
        ESP_LOGE(TAG, "OTA message needs \"url\" and \"hash\"");
        return;
    }
    if (strlen(hash) != 64) {
        ESP_LOGE(TAG, "hash must be 64 hex chars (SHA-256), got %u", (unsigned)strlen(hash));
        return;
    }

    _inProgress = true;
    ESP_LOGI(TAG, "OTA notification received. url=%s", url);

    bool ok = downloadAndVerify(String(url), String(hash));
    if (ok) {
        ESP_LOGI(TAG, "Download complete, hash verified OK.");
        _lastUrl  = url;
        _lastHash = hash;
        applyUpdate();          // reboots on success; returns only on failure
    } else {
        ESP_LOGE(TAG, "OTA download/verification FAILED - update aborted. Running firmware untouched.");
    }

    _inProgress = false;
}

// ---------------------------------------------------------------------------
bool OtaManager::downloadAndVerify(const String& url, const String& expectedHashHex) {
    s_ready  = false;
    s_handle = 0;

    const bool isHttps = url.startsWith("https://");
    const bool isHttp  = url.startsWith("http://");
    if (!isHttps && !(isHttp && OTA_ALLOW_HTTP)) {
        ESP_LOGE(TAG, "URL scheme not allowed: %s", url.c_str());
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGE(TAG, "WiFi not connected");
        return false;
    }

    s_target = esp_ota_get_next_update_partition(nullptr);
    if (!s_target) {
        ESP_LOGE(TAG, "No OTA target partition");
        return false;
    }

    WiFiClient       plainClient;
    WiFiClientSecure secureClient;
    HTTPClient       http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    bool began;
    if (isHttps) {
#if OTA_ALLOW_INSECURE_TLS
        secureClient.setInsecure();     // TESTING ONLY - use setCACert() in production
#endif
        began = http.begin(secureClient, url);
    } else {
        began = http.begin(plainClient, url);
    }
    if (!began) {
        ESP_LOGE(TAG, "http.begin() failed");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        ESP_LOGE(TAG, "HTTP GET failed, code=%d", code);
        http.end();
        return false;
    }

    const int total = http.getSize();
    if (total <= 0) {
        ESP_LOGE(TAG, "Server did not send Content-Length - refusing");
        http.end();
        return false;
    }
    if ((size_t)total > s_target->size) {
        ESP_LOGE(TAG, "Firmware (%d bytes) does not fit slot (%u bytes)", total, (unsigned)s_target->size);
        http.end();
        return false;
    }
    ESP_LOGI(TAG, "Downloading %d bytes into %s ...", total, s_target->label);

    esp_err_t e = esp_ota_begin(s_target, OTA_WITH_SEQUENTIAL_WRITES, &s_handle);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(e));
        http.end();
        return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);     // 0 = SHA-256

    WiFiClient* stream = http.getStreamPtr();
    uint8_t  buf[1024];
    int      written  = 0;
    uint32_t lastData = millis();
    uint32_t lastLog  = 0;
    bool     ok       = true;

    while (written < total) {
        if (!http.connected() && stream->available() == 0) {
            ESP_LOGE(TAG, "Connection closed early at %d / %d bytes", written, total);
            ok = false;
            break;
        }
        size_t avail = stream->available();
        if (avail == 0) {
            if (millis() - lastData > OTA_STALL_TIMEOUT_MS) {
                ESP_LOGE(TAG, "Download stalled at %d / %d bytes", written, total);
                ok = false;
                break;
            }
            delay(5);
            continue;
        }
        size_t want = avail > sizeof(buf) ? sizeof(buf) : avail;
        if ((int)want > total - written) want = total - written;
        int n = stream->readBytes(buf, want);
        if (n <= 0) continue;
        lastData = millis();

        e = esp_ota_write(s_handle, buf, n);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(e));
            ok = false;
            break;
        }
        mbedtls_sha256_update(&sha, buf, n);
        written += n;

        if (millis() - lastLog > 2000) {
            ESP_LOGI(TAG, "Progress: %d / %d bytes (%d%%)", written, total, (int)(100LL * written / total));
            lastLog = millis();
        }
    }
    http.end();

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);

    if (ok) {
        String got = toHex(digest, 32);
        String want = expectedHashHex;
        want.toLowerCase();
        ESP_LOGI(TAG, "Computed hash: %s", got.c_str());
        if (got != want) {
            ESP_LOGE(TAG, "HASH MISMATCH. expected: %s", want.c_str());
            ok = false;
        } else {
            ESP_LOGI(TAG, "Hash matches.");
        }
    }

    if (!ok) {
        esp_ota_abort(s_handle);    // inactive slot discarded; boot partition untouched
        s_handle = 0;
        return false;
    }

    s_ready = true;                 // fully written + verified, NOT yet activated
    return true;
}

// ---------------------------------------------------------------------------
void OtaManager::applyUpdate() {
    if (!s_ready || !s_target) {
        ESP_LOGE(TAG, "applyUpdate() called without a verified download - refusing");
        return;
    }

    esp_err_t e = esp_ota_end(s_handle);        // validates the written image
    s_handle = 0;
    s_ready  = false;
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed (image invalid?): %s", esp_err_to_name(e));
        return;
    }

    // This single call is the commit point: before it the old firmware still boots,
    // after it the new one does (and is "pending verify" until it confirms itself).
    e = esp_ota_set_boot_partition(s_target);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(e));
        return;
    }

    ESP_LOGI(TAG, "Update committed. Boot partition = %s. Rebooting...", s_target->label);
    delay(500);
    esp_restart();
}

// ---------------------------------------------------------------------------
void OtaManager::checkAndConfirmBoot() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        ESP_LOGI(TAG, "No OTA state for running partition (normal after a USB flash)");
        return;
    }
    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Image state=%d (not pending verify) - nothing to confirm", (int)state);
        return;
    }

    ESP_LOGW(TAG, "New firmware booted (PENDING VERIFY). Waiting up to %d s for WiFi + MQTT...",
             OTA_BOOT_CHECK_TIMEOUT_MS / 1000);

    bool wifiOk = false, mqttOk = false;
    uint32_t start = millis();
    while (millis() - start < OTA_BOOT_CHECK_TIMEOUT_MS) {
        wifiOk = (WiFi.status() == WL_CONNECTED);
        mqttOk = MqttManager::instance().isConnected();
        if (wifiOk && mqttOk) break;
        delay(500);
    }

    ESP_LOGI(TAG, "Check: WiFi=%s, MQTT=%s", wifiOk ? "OK" : "FAIL", mqttOk ? "OK" : "FAIL");

    if (wifiOk && mqttOk) {
        ESP_LOGI(TAG, "Validation PASSED. Marking firmware valid (rollback cancelled).");
        esp_ota_mark_app_valid_cancel_rollback();
    } else {
        ESP_LOGE(TAG, "Validation FAILED. Rolling back to previous firmware...");
        delay(300);
        esp_ota_mark_app_invalid_rollback_and_reboot();   // does not return
    }
}
