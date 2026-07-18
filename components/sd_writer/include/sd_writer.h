#ifndef SD_WRITER_H
#define SD_WRITER_H

#include "esp_err.h"

/**
 * @brief Initialize the SD writer component.
 *        This mounts the SD card, creates the logging queue, starts the SD write worker task,
 *        and registers the event handler to listen for CAN events.
 * 
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t sd_writer_init(void);

#endif // SD_WRITER_H
