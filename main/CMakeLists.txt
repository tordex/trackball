idf_component_register(SRCS "led/esp_led.cpp"
                            "led/led_strip_encoder.c"
                            "button/button.cpp"
                            "main.cpp"
                            "nimble/ble_func.c"
                            "nimble/gatt_svr.c"
                            "nimble/gatt_vars.c"
                            "nimble/hid_func.c"
                            "paw3395/paw3395.cpp"

                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio driver esp_driver_rmt
                    INCLUDE_DIRS "." "led" "paw3395" "nimble" "button")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
