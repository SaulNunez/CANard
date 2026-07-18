#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include "esp_err.h"

/**
 * @brief Initialize the CAN manager component.
 *        This installs the TWAI/CAN driver, starts it, and spawns the RX task.
 * 
 * @return esp_err_t ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t can_manager_init(void);

#endif // CAN_MANAGER_H
