#include "sd_writer.h"
#include "can_events.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static const char *TAG = "sd_writer";

// Queue to hold received CAN messages
static QueueHandle_t sd_write_queue = NULL;

// SPI pin defaults if not defined
#ifndef PIN_NUM_MISO
#define PIN_NUM_MISO 2
#endif
#ifndef PIN_NUM_MOSI
#define PIN_NUM_MOSI 15
#endif
#ifndef PIN_NUM_CLK
#define PIN_NUM_CLK  14
#endif
#ifndef PIN_NUM_CS
#define PIN_NUM_CS   13
#endif

// SD card struct pointer
static sdmmc_card_t* s_card = NULL;

// File path for logging
#define LOG_FILE_PATH "/sdcard/can_log.txt"

// Worker task to write to SD card
static void sd_write_task(void *pvParameters) {
    ESP_LOGI(TAG, "SD write worker task started");
    twai_message_t message;
    
    while (true) {
        // Block indefinitely until a message arrives in the queue
        if (xQueueReceive(sd_write_queue, &message, portMAX_DELAY) == pdTRUE) {
            FILE* f = fopen(LOG_FILE_PATH, "a");
            if (f == NULL) {
                ESP_LOGE(TAG, "Failed to open log file %s for writing", LOG_FILE_PATH);
                continue;
            }

            // Print standard message headers
            fprintf(f, "ID: 0x%03" PRIx32 " | DLC: %d | Format: %s | Type: %s | Data:",
                    message.identifier,
                    message.data_length_code,
                    (message.flags & TWAI_MSG_FLAG_EXTD) ? "Extended" : "Standard",
                    (message.flags & TWAI_MSG_FLAG_RTR) ? "RTR" : "Data");

            if (!(message.flags & TWAI_MSG_FLAG_RTR)) {
                for (int i = 0; i < message.data_length_code; i++) {
                    fprintf(f, " 0x%02X", message.data[i]);
                }
            }
            fprintf(f, "\n");
            fclose(f);

            // Also echo to console for visibility as in original main.c
            if (message.flags & TWAI_MSG_FLAG_EXTD) {
                printf("Message is in Extended Format\n");
            } else {
                printf("Message is in Standard Format\n");
            }
            printf("ID is %" PRIu32 "\n", message.identifier);
            if (!(message.flags & TWAI_MSG_FLAG_RTR)) {
                for (int i = 0; i < message.data_length_code; i++) {
                    printf("Data byte %d = %d\n", i, message.data[i]);
                }
            }
        }
    }
}

// Event handler callback
static void can_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    if (base == CAN_EVENTS && id == CAN_EVENT_MESSAGE_RECEIVED) {
        twai_message_t* message = (twai_message_t*)event_data;
        // Non-blocking send to the queue. System Event loop task must not block!
        if (xQueueSend(sd_write_queue, message, 0) != pdTRUE) {
            ESP_LOGW(TAG, "SD write queue full, dropping CAN message ID: 0x%03" PRIx32, message->identifier);
        }
    }
}

esp_err_t sd_writer_init(void) {
    esp_err_t ret;

    #ifndef USE_SPI_MODE
        ESP_LOGI(TAG, "Using SDMMC peripheral");
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

        gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);   // CMD
        gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);    // D0
        gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);    // D1
        gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);   // D2
        gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);   // D3
    #else
        ESP_LOGI(TAG, "Using SPI peripheral");
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
        slot_config.gpio_miso = PIN_NUM_MISO;
        slot_config.gpio_mosi = PIN_NUM_MOSI;
        slot_config.gpio_sck  = PIN_NUM_CLK;
        slot_config.gpio_cs   = PIN_NUM_CS;
    #endif

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return ret;
    }

    ESP_LOGI(TAG, "SD Card mounted successfully, Name: %s", s_card->cid.name);

    // Create FreeRTOS Queue for CAN messages
    sd_write_queue = xQueueCreate(100, sizeof(twai_message_t));
    if (sd_write_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create SD write queue");
        return ESP_ERR_NO_MEM;
    }

    // Spawn the worker task to write to SD card
    #ifndef APP_CPU_NUM
    #define APP_CPU_NUM 1
    #endif

    BaseType_t xReturned = xTaskCreatePinnedToCore(
        sd_write_task,
        "sd_write_task",
        4096,
        NULL,
        tskIDLE_PRIORITY + 1, // Low priority task so it doesn't block critical tasks
        NULL,
        APP_CPU_NUM
    );

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create SD write task");
        vQueueDelete(sd_write_queue);
        sd_write_queue = NULL;
        return ESP_FAIL;
    }

    // Register event handler on the default event loop
    ret = esp_event_handler_register(CAN_EVENTS, CAN_EVENT_MESSAGE_RECEIVED, &can_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD Writer initialized successfully");
    return ESP_OK;
}
