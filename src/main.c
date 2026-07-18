#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sd_writer.h"
#include "can_manager.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "Initializing CANard system...");

    // Initialize NVS Flash (required for system event loop in ESP-IDF)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create the Default Event Loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize the SD card writer subsystem (registers handler for CAN events)
    ESP_ERROR_CHECK(sd_writer_init());

    // Initialize the CAN manager subsystem (starts CAN and RX task, posts CAN events)
    ESP_ERROR_CHECK(can_manager_init());

    ESP_LOGI(TAG, "System initialization complete. Event loop running.");
}