FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.c)

idf_component_register(SRCS ${app_sources}
                       PRIV_REQUIRES can_manager sd_writer bluetooth_spp nvs_flash esp_event)
