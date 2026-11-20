/*
 * ESP32-C5 Zigbee Coordinator
 *
 * Features:
 * - Forms Zigbee coordinator network
 * - Opens network for joining
 * - Logs ALL major Zigbee callbacks
 * - Logs joined devices
 * - Sends ON/OFF commands to joined light device
 * - Thread-safe Zigbee API usage
 * - Button-controlled ON/OFF toggle
 * - NEW: Automatically binds the first two joined routers to each other!
 *
 * Tested with:
 * ESP-IDF        : v5.5.x
 * esp-zigbee-lib : 1.6.x
 */

// ============================================================== NEW CODE

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "esp_zigbee_core.h"
#include "ha/esp_zigbee_ha_standard.h"

/* ------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------ */

#define TAG "ZB_COORD"

#define COORDINATOR_ENDPOINT 1
#define LIGHT_ENDPOINT 10 // Must match the endpoint on your Routers

#define BUTTON_GPIO 28

#define MAX_CHILDREN 10
#define ZIGBEE_CHANNEL 11

/* Open network forever */
#define PERMIT_JOIN_DURATION 0xFF

/* ------------------------------------------------------------
 * Globals
 * ------------------------------------------------------------ */

static uint16_t joined_device_short_addr[MAX_CHILDREN] = {[0 ... MAX_CHILDREN - 1] = 0xFFFF};
static esp_zb_ieee_addr_t joined_device_ieee_addr[MAX_CHILDREN]; // Added to store IEEE addresses for binding
static uint8_t joined_device_count = 0;

static bool light_state = false;
static bool devices_bound = false; // Flag to ensure we only bind once

/* ------------------------------------------------------------
 * ZDO Bind Callback Handler
 * ------------------------------------------------------------ */

static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "ZDO Binding Request successful!");
    }
    else
    {
        ESP_LOGE(TAG, "ZDO Binding Request failed, status: %d", zdo_status);
    }
}

/* ------------------------------------------------------------
 * Helper: Bind Devices by MAC Address
 * ------------------------------------------------------------ */
static void bind_devices_by_mac(
    uint16_t target_short_addr,
    esp_zb_ieee_addr_t source_mac,
    uint8_t source_endpoint,
    esp_zb_ieee_addr_t dest_mac,
    uint8_t dest_endpoint,
    uint16_t cluster_id)
{
    esp_zb_zdo_bind_req_param_t bind_req;
    memset(&bind_req, 0, sizeof(esp_zb_zdo_bind_req_param_t));

    bind_req.req_dst_addr = target_short_addr;
    memcpy(bind_req.src_address, source_mac, sizeof(esp_zb_ieee_addr_t));
    bind_req.src_endp = source_endpoint;
    bind_req.cluster_id = cluster_id;

    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req.dst_address_u.addr_long, dest_mac, sizeof(esp_zb_ieee_addr_t));
    bind_req.dst_endp = dest_endpoint;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zdo_device_bind_req(&bind_req, bind_cb, NULL);
    esp_zb_lock_release();
}

/* ------------------------------------------------------------
 * Generic Zigbee callback logger
 * ------------------------------------------------------------ */

static esp_err_t zb_action_handler(
    esp_zb_core_action_callback_id_t callback_id,
    const void *message)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ZIGBEE CALLBACK");
    ESP_LOGI(TAG, "Callback ID: %d", callback_id);

    switch (callback_id)
    {
    case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
    {
        const esp_zb_zcl_report_attr_message_t *report =
            (const esp_zb_zcl_report_attr_message_t *)message;

        ESP_LOGI(TAG, "ATTRIBUTE REPORT");
        ESP_LOGI(TAG, "Src Addr : 0x%04x", report->src_address.u.short_addr);
        ESP_LOGI(TAG, "Src EP   : %d", report->src_endpoint);
        ESP_LOGI(TAG, "Dst EP   : %d", report->dst_endpoint);
        ESP_LOGI(TAG, "Cluster  : 0x%04x", report->cluster);
        ESP_LOGI(TAG, "Attr ID  : 0x%04x", report->attribute.id);
        ESP_LOGI(TAG, "Type     : 0x%02x", report->attribute.data.type);

        if (report->attribute.data.value != NULL)
        {
            switch (report->attribute.data.type)
            {
            case ESP_ZB_ZCL_ATTR_TYPE_BOOL:
                ESP_LOGI(TAG, "Value(BOOL): %d", *(bool *)report->attribute.data.value);
                break;
            case ESP_ZB_ZCL_ATTR_TYPE_U8:
                ESP_LOGI(TAG, "Value(U8): %u", *(uint8_t *)report->attribute.data.value);
                break;
            case ESP_ZB_ZCL_ATTR_TYPE_U16:
                ESP_LOGI(TAG, "Value(U16): %u", *(uint16_t *)report->attribute.data.value);
                break;
            default:
                ESP_LOGI(TAG, "Unhandled attribute type");
                break;
            }
        }
        break;
    }

    case ESP_ZB_CORE_CMD_READ_ATTR_RESP_CB_ID:
        ESP_LOGI(TAG, "READ ATTRIBUTE RESPONSE");
        break;

    case ESP_ZB_CORE_CMD_REPORT_CONFIG_RESP_CB_ID:
        ESP_LOGI(TAG, "REPORT CONFIG RESPONSE");
        break;

    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        ESP_LOGI(TAG, "DEFAULT RESPONSE");
        break;

    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ESP_LOGI(TAG, "SET ATTRIBUTE VALUE");
        break;

    default:
        ESP_LOGW(TAG, "Unhandled callback ID: %d", callback_id);
        break;
    }

    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}
/* ------------------------------------------------------------
 * Asynchronous Binding Task
 * ------------------------------------------------------------ */
typedef struct
{
    int dev1_idx;
    int dev2_idx;
} bind_args_t;

static void async_bind_task(void *pvParameters)
{
    bind_args_t *args = (bind_args_t *)pvParameters;
    int dev1 = args->dev1_idx;
    int dev2 = args->dev2_idx;
    free(args); // Free the memory passed to the task

    ESP_LOGI(TAG, "Two devices joined! Waiting 6 seconds for network to settle...");
    vTaskDelay(pdMS_TO_TICKS(6000));

    ESP_LOGI(TAG, "Sending cross-binding requests using DYNAMIC MAC Addresses...");

    /* --- Bind Router 1 (Client 11) to Router 2 (Server 10) --- */
    bind_devices_by_mac(
        joined_device_short_addr[dev1], // Target getting the update
        joined_device_ieee_addr[dev1],  // Source MAC (Router 1)
        11,                             // Source Endpoint (Switch)
        joined_device_ieee_addr[dev2],  // Dest MAC (Router 2)
        10,                             // Dest Endpoint (Light)
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF    // Cluster ID
    );

    /* Space out the radio transmissions so buffers don't overflow */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* --- Bind Router 2 (Client 11) to Router 1 (Server 10) --- */
    bind_devices_by_mac(
        joined_device_short_addr[dev2], // Target getting the update
        joined_device_ieee_addr[dev2],  // Source MAC (Router 2)
        11,                             // Source Endpoint (Switch)
        joined_device_ieee_addr[dev1],  // Dest MAC (Router 1)
        10,                             // Dest Endpoint (Light)
        ESP_ZB_ZCL_CLUSTER_ID_ON_OFF    // Cluster ID
    );

    ESP_LOGI(TAG, "Dynamic cross-binding requests sent successfully!");

    // Delete this task now that its job is done
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------
 * Callback: Handles the response containing the MAC address
 * ------------------------------------------------------------ */
static void ieee_addr_req_cb(esp_zb_zdp_status_t zdo_status, esp_zb_zdo_ieee_addr_rsp_t *rsp, void *user_ctx)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS)
    {
        ESP_LOGI(TAG, "SUCCESS! Device 0x%04x responded with its MAC Address.", rsp->nwk_addr);

        // 1. Save the newly fetched MAC address into our global array
        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            if (joined_device_short_addr[i] == rsp->nwk_addr)
            {
                memcpy(joined_device_ieee_addr[i], rsp->ieee_addr, sizeof(esp_zb_ieee_addr_t));
                break;
            }
        }

        // 2. Check if we have two devices with valid MAC addresses ready to bind
        int dev1 = -1, dev2 = -1;
        for (int i = 0; i < MAX_CHILDREN; i++)
        {

            // Check if the MAC address array isn't completely empty (zeros)
            bool has_mac = false;
            for (int j = 0; j < 8; j++)
            {
                if (joined_device_ieee_addr[i][j] != 0)
                    has_mac = true;
            }

            if (joined_device_short_addr[i] != 0xFFFF && has_mac)
            {
                if (dev1 == -1)
                    dev1 = i;
                else if (dev2 == -1)
                {
                    dev2 = i;
                    break;
                }
            }
        }

        // 3. If we have 2 complete devices, launch the binding task!
        if (dev1 != -1 && dev2 != -1 && !devices_bound)
        {
            devices_bound = true; // Lock it so we don't bind again
            ESP_LOGI(TAG, "Two devices successfully queried! Launching bind task...");

            bind_args_t *args = malloc(sizeof(bind_args_t));
            if (args != NULL)
            {
                args->dev1_idx = dev1;
                args->dev2_idx = dev2;
                xTaskCreate(async_bind_task, "bind_task", 4096, args, 5, NULL);
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to get IEEE address. Status code: 0x%x", zdo_status);
    }
}

/* ------------------------------------------------------------
 * Function: Ask a device for its MAC Address
 * ------------------------------------------------------------ */
static void query_device_mac_address(uint16_t target_short_addr)
{
    ESP_LOGI(TAG, "Asking device 0x%04x for its MAC address over the air...", target_short_addr);

    esp_zb_zdo_ieee_addr_req_param_t req;
    memset(&req, 0, sizeof(esp_zb_zdo_ieee_addr_req_param_t));

    req.dst_nwk_addr = target_short_addr;     // Who are we sending the packet to?
    req.addr_of_interest = target_short_addr; // Whose MAC address do we want? (Theirs)
    req.request_type = 0;                     // 0 = Single device response
    req.start_index = 0;

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zdo_ieee_addr_req(&req, ieee_addr_req_cb, NULL);
    esp_zb_lock_release();
}

/* ------------------------------------------------------------
 * Zigbee signal handler
 * ------------------------------------------------------------ */

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig = *p_sg_p;

    switch (sig)
    {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
    {
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    }

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
    {
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Coordinator started");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        }
        else
        {
            ESP_LOGE(TAG, "Startup failed: %s", esp_err_to_name(err_status));
        }
        break;
    }

    case ESP_ZB_BDB_SIGNAL_FORMATION:
    {
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Network formed");
            ESP_LOGI(TAG, "PAN ID: 0x%04x", esp_zb_get_pan_id());
            ESP_LOGI(TAG, "Channel: %d", esp_zb_get_current_channel());
            ESP_LOGI(TAG, "Short Addr: 0x%04x", esp_zb_get_short_address());

            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        break;
    }

    case ESP_ZB_BDB_SIGNAL_STEERING:
    {
        if (err_status == ESP_OK)
        {
            ESP_LOGI(TAG, "Network open for joining");
            esp_zb_bdb_open_network(PERMIT_JOIN_DURATION);
        }
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
    {
        esp_zb_zdo_signal_device_annce_params_t *dev =
            (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);

        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "DEVICE JOINED");
        ESP_LOGI(TAG, "Short Addr : 0x%04x", dev->device_short_addr);

        ESP_LOGI(TAG, "Capability : 0x%02x", dev->capability);
        ESP_LOGI(TAG, "Device Type: %s", (dev->capability & (1 << 1)) ? "Router" : "End Device");
        ESP_LOGI(TAG, "========================================");

        // Find an empty slot and save the Short Address
        bool already_exists = false;
        int empty_idx = -1;
        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            if (joined_device_short_addr[i] == dev->device_short_addr)
            {
                already_exists = true;
                break;
            }
            if (joined_device_short_addr[i] == 0xFFFF && empty_idx == -1)
            {
                empty_idx = i;
            }
        }

        if (!already_exists && empty_idx != -1)
        {
            joined_device_short_addr[empty_idx] = dev->device_short_addr;
            joined_device_count++;

            // Clear out any old MAC address sitting in memory for this slot
            memset(joined_device_ieee_addr[empty_idx], 0, sizeof(esp_zb_ieee_addr_t));

            // FIRE THE QUERY!
            query_device_mac_address(dev->device_short_addr);
        }
        break;
    }

    case ESP_ZB_ZDO_SIGNAL_LEAVE:
    {
        esp_zb_zdo_signal_device_annce_params_t *dev =
            (esp_zb_zdo_signal_device_annce_params_t *)
                esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGW(TAG, "Device left network");

        for (int i = 0; i < MAX_CHILDREN; i++)
        {
            if (joined_device_short_addr[i] == dev->device_short_addr)
            {
                joined_device_short_addr[i] = 0xFFFF;
                memset(joined_device_ieee_addr[i], 0, sizeof(esp_zb_ieee_addr_t)); // Clear MAC address
                joined_device_count--;
                devices_bound = false; // Reset so they can re-bind if joining again
                break;
            }
        }

        break;
    }

    default:
    {
        ESP_LOGD(TAG, "Signal: %d Status: %s", sig, esp_err_to_name(err_status));
        break;
    }
    }
}

/* ------------------------------------------------------------
 * Create Coordinator Endpoint
 * ------------------------------------------------------------ */

static void zb_create_coordinator_endpoint(void)
{
    esp_zb_on_off_switch_cfg_t switch_cfg =
        ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();

    esp_zb_ep_list_t *ep_list =
        esp_zb_on_off_switch_ep_create(
            COORDINATOR_ENDPOINT,
            &switch_cfg);

    esp_zb_device_register(ep_list);

    esp_zb_core_action_handler_register(
        zb_action_handler);

    ESP_LOGI(TAG, "Coordinator endpoint created");
}

/* ------------------------------------------------------------
 * Send ON/OFF command
 * ------------------------------------------------------------ */

static void send_on_off_command(bool on)
{
    if (joined_device_short_addr[0] == 0xFFFF)
    {
        ESP_LOGW(TAG, "No joined device");
        return;
    }

    for (int i = 0; i < MAX_CHILDREN; i++)
    {
        if (joined_device_short_addr[i] != 0xFFFF)
        {
            ESP_LOGI(TAG, "Sending %s command to 0x%04x ep %d",
                     on ? "ON" : "OFF",
                     joined_device_short_addr[i],
                     LIGHT_ENDPOINT);

            esp_zb_zcl_on_off_cmd_t cmd_req = {
                .zcl_basic_cmd =
                    {
                        .dst_addr_u.addr_short = joined_device_short_addr[i],
                        .dst_endpoint = LIGHT_ENDPOINT,
                        .src_endpoint = COORDINATOR_ENDPOINT,
                    },
                .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                .on_off_cmd_id = on ? ESP_ZB_ZCL_CMD_ON_OFF_ON_ID : ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID,
            };

            esp_zb_lock_acquire(portMAX_DELAY);
            esp_err_t err = esp_zb_zcl_on_off_cmd_req(&cmd_req);
            esp_zb_lock_release();

            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "Command queued successfully");
            }
        }
    }
}

/* ------------------------------------------------------------
 * Button Task
 * ------------------------------------------------------------ */

void button_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };

    gpio_config(&io_conf);

    bool last_state = true;

    ESP_LOGI(TAG, "Button task started on GPIO %d", BUTTON_GPIO);

    while (1)
    {
        bool current = gpio_get_level(BUTTON_GPIO);

        if (last_state && !current)
        {
            vTaskDelay(pdMS_TO_TICKS(50));

            if (!gpio_get_level(BUTTON_GPIO))
            {
                ESP_LOGI(TAG, "Button pressed");

                if (joined_device_count == 0)
                {
                    ESP_LOGW(TAG, "No device joined yet");
                }
                else
                {
                    light_state = !light_state;
                    send_on_off_command(light_state);
                }

                while (!gpio_get_level(BUTTON_GPIO))
                {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
        }

        last_state = current;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ------------------------------------------------------------
 * Zigbee Main Task
 * ------------------------------------------------------------ */

void esp_zb_task(void *pvParameters)
{
    esp_zb_platform_config_t platform_cfg = {
        .radio_config =
            {
                .radio_mode = ZB_RADIO_MODE_NATIVE,
            },

        .host_config =
            {
                .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,
            },
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_cfg));

    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
        .install_code_policy = false,
        .nwk_cfg =
            {
                .zczr_cfg =
                    {
                        .max_children = MAX_CHILDREN,
                    },
            },
    };

    esp_zb_init(&zb_nwk_cfg);
    zb_create_coordinator_endpoint();
    esp_zb_set_channel_mask(1UL << ZIGBEE_CHANNEL);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();

    vTaskDelete(NULL);
}
