#ifndef BLUETOOTH_SPP_H
#define BLUETOOTH_SPP_H

#include "esp_err.h"

/**
 * @brief Initialize the Bluetooth SPP serial component.
 *        This initializes BT, Bluedroid, sets up GAP and SPP, and registers
 *        the event loop listener to forward CAN frames.
 * 
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t bluetooth_spp_init(void);

#endif // BLUETOOTH_SPP_H
