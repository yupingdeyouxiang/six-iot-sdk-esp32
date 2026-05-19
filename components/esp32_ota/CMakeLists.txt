idf_component_register(SRCS "six_iot_ota.c"
                    REQUIRES esp_https_ota app_update esp32_conf esp_http_client
                    INCLUDE_DIRS "include"
                    )

