idf_component_register(SRCS "led/esp_led.cpp"
                            "led/led_strip_encoder.c"
                            "button/button.cpp"
                            "main.cpp"
                            "ble/esp_hidd_prf_api.c"
                            "ble/hid_dev.c"
                            "ble/hid_device_le_prf.c"
                            "ble/ble_init.c"
                            "paw3395/paw3395.cpp"

                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio driver esp_driver_rmt
                    INCLUDE_DIRS "." "led" "paw3395" "ble" "button")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
