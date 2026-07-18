#include "bluetooth_spp.h"
#include "can_events.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bluetooth_spp";
static uint32_t spp_conn_handle = 0;
static bool spp_connected = false;

static const esp_spp_mode_t spp_mode = ESP_SPP_MODE_CB;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "SPP initialized, starting server");
            esp_spp_start_srv(sec_mask, role_slave, 0, "SPP_SERVER");
        } else {
            ESP_LOGE(TAG, "SPP init failed: status %d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        break;
    case ESP_SPP_OPEN_EVT:
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(TAG, "SPP connection closed");
        spp_conn_handle = 0;
        spp_connected = false;
        break;
    case ESP_SPP_START_EVT:
        ESP_LOGI(TAG, "SPP server started");
        break;
    case ESP_SPP_CL_INIT_EVT:
        break;
    case ESP_SPP_DATA_IND_EVT:
        // Remote sent us data (echo it or log it)
        ESP_LOGI(TAG, "Received %d bytes over SPP", param->data_ind.len);
        break;
    case ESP_SPP_CONG_EVT:
        break;
    case ESP_SPP_WRITE_EVT:
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        if (param->srv_open.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "SPP connection opened (Client connected)");
            spp_conn_handle = param->srv_open.handle;
            spp_connected = true;
        } else {
            ESP_LOGE(TAG, "SPP connection open failed: status %d", param->srv_open.status);
        }
        break;
    case ESP_SPP_UNCONG_EVT:
        break;
    default:
        break;
    }
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Authentication success: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "Authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    case ESP_BT_GAP_CFG_DEV_CLASS_EVT:
        break;
    default:
        break;
    }
}

// Event handler to listen for received CAN messages and transmit them over SPP
static void can_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    if (base == CAN_EVENTS && id == CAN_EVENT_MESSAGE_RECEIVED) {
        twai_message_t* message = (twai_message_t*)event_data;
        
        if (spp_connected && spp_conn_handle != 0) {
            uint8_t tx_buf[20];
            int idx = 0;
            
            // Start delimiter
            tx_buf[idx++] = 0xAA;
            
            // Timestamp in microseconds (4 bytes, little-endian)
            uint32_t time_us = (uint32_t)esp_timer_get_time();
            tx_buf[idx++] = (uint8_t)(time_us & 0xFF);
            tx_buf[idx++] = (uint8_t)((time_us >> 8) & 0xFF);
            tx_buf[idx++] = (uint8_t)((time_us >> 16) & 0xFF);
            tx_buf[idx++] = (uint8_t)((time_us >> 24) & 0xFF);
            
            // DLC (1 byte)
            tx_buf[idx++] = message->data_length_code;
            
            // Arbitration ID (4 bytes, little-endian)
            uint32_t arb_id = message->identifier;
            tx_buf[idx++] = (uint8_t)(arb_id & 0xFF);
            tx_buf[idx++] = (uint8_t)((arb_id >> 8) & 0xFF);
            tx_buf[idx++] = (uint8_t)((arb_id >> 16) & 0xFF);
            tx_buf[idx++] = (uint8_t)((arb_id >> 24) & 0xFF);
            
            // Payload (0-8 bytes)
            for (int i = 0; i < message->data_length_code && i < 8; i++) {
                tx_buf[idx++] = message->data[i];
            }
            
            // End delimiter
            tx_buf[idx++] = 0xBB;
            
            // Write data over SPP
            esp_spp_write(spp_conn_handle, idx, tx_buf);
        }
    }
}

esp_err_t bluetooth_spp_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing Bluetooth controller");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Initializing Bluedroid stack");
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid stack init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid stack enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Registering GAP and SPP callbacks");
    ret = esp_bt_gap_register_callback(esp_bt_gap_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_spp_register_callback(esp_spp_cb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPP register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_spp_init(spp_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPP init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set device name and scan mode so it's discoverable
    esp_bt_gap_set_device_name("CANard-BT");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // Register our event loop listener for CAN events to stream them over BT SPP
    ret = esp_event_handler_register(CAN_EVENTS, CAN_EVENT_MESSAGE_RECEIVED, &can_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register BT CAN event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Bluetooth SPP initialized successfully, device name: 'CANard-BT'");
    return ESP_OK;
}
