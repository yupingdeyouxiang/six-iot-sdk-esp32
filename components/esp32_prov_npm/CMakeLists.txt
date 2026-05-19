idf_component_register(SRCS "six_iot_network_prov_manager.c"
                    REQUIRES nvs_flash esp32_cred esp32_conf esp32_util protobuf-c esp32_proto
                    INCLUDE_DIRS "include"
                    )
