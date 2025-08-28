idf_component_register(SRCS "esp_led.cpp"
                            "led_strip_encoder.c"
                            "button.cpp"
                            "main.cpp"
                            "esp_hidd_prf_api.c"
                            "hid_dev.c"
                            "hid_device_le_prf.c"
                            "ble_init.c"
                            "paw3395.cpp"

                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio driver esp_driver_rmt
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
