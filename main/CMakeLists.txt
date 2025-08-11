idf_component_register(SRCS "button.cpp"
                            "main.cpp"
                            "esp_hidd_prf_api.c"
                            "hid_dev.c"
                            "hid_device_le_prf.c"
                            "ble_init.c"
                            "paw3395.cpp"

                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio driver
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
