idf_component_register(SRCS "blufi_init.c" "blufi_security.c" "six_iot_prov_blufi.c"
                    REQUIRES bt esp32_conf nvs_flash
                    INCLUDE_DIRS "include")

