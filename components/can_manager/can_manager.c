#include "can_manager.h"
#include "can_events.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "can_manager";

// Define the event base
ESP_EVENT_DEFINE_BASE(CAN_EVENTS);

static void can_receive_task(void *pvParameters) {
    ESP_LOGI(TAG, "CAN/TWAI receive task started");
    while (true) {
        twai_message_t message;
        // Wait for a message
        esp_err_t ret = twai_receive(&message, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK) {
            ESP_LOGD(TAG, "CAN message received, ID: 0x%03" PRIx32, message.identifier);
            // Post the received message to the default event loop
            esp_err_t err = esp_event_post(CAN_EVENTS, CAN_EVENT_MESSAGE_RECEIVED, &message, sizeof(message), portMAX_DELAY);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to post CAN event: %s", esp_err_to_name(err));
            }
        } else if (ret == ESP_ERR_TIMEOUT) {
            // Normal timeout, loop again
        } else {
            ESP_LOGE(TAG, "Error receiving CAN message: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

esp_err_t can_manager_init(void) {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_21, GPIO_NUM_22, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install TWAI driver
    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start TWAI driver
    ret = twai_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(ret));
        // Try to uninstall to clean up
        twai_driver_uninstall();
        return ret;
    }

    ESP_LOGI(TAG, "TWAI driver installed and started successfully");

    // Spawn CAN receive task
    // Using xTaskCreate since it's standard. We can pin it to core 1 (or APP_CPU_NUM if defined, otherwise 1)
    #ifndef APP_CPU_NUM
    #define APP_CPU_NUM 1
    #endif

    BaseType_t xReturned = xTaskCreatePinnedToCore(
        can_receive_task,
        "can_rx_task",
        4096,
        NULL,
        configMAX_PRIORITIES - 1, // High priority
        NULL,
        APP_CPU_NUM
    );

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN receive task");
        return ESP_FAIL;
    }

    return ESP_OK;
}
