idf_component_register(SRCS "six_iot_util.c" "six_iot_nvs.c"
                    REQUIRES mbedtls json nvs_flash esp_wifi 
                    INCLUDE_DIRS "include")

