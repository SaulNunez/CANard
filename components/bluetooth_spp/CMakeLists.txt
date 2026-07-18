idf_component_register(SRCS "bluetooth_spp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bt esp_event esp_timer can_manager)
