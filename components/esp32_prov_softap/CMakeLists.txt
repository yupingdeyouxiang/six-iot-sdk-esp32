idf_component_register(SRCS "six_iot_prov_softap.c"
					REQUIRES esp32_conf esp_http_server esp_wifi nvs_flash
                    INCLUDE_DIRS "include"
                    EMBED_FILES "wifi.html"
                    )

