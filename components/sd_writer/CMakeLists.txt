idf_component_register(SRCS "sd_writer.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver fatfs sdmmc esp_event can_manager esp_timer)
