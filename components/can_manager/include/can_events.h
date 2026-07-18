#ifndef CAN_EVENTS_H
#define CAN_EVENTS_H

#include "sdkconfig.h"
#ifdef CONFIG_TWAI_SUPPRESS_DEPRECATE_WARN
#undef CONFIG_TWAI_SUPPRESS_DEPRECATE_WARN
#endif
#define CONFIG_TWAI_SUPPRESS_DEPRECATE_WARN 1

#include "esp_event.h"
#include "driver/twai.h"

// Declare the event base
ESP_EVENT_DECLARE_BASE(CAN_EVENTS);

// Event IDs
enum {
    CAN_EVENT_MESSAGE_RECEIVED,
};

#endif // CAN_EVENTS_H
