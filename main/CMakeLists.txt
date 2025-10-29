idf_component_register(SRCS "app.cpp"
                            "ui.cpp"
                            "button/button.cpp"
                            "main.cpp"
                            "nimble/ble_func.c"
                            "nimble/gatt_svr.c"
                            "nimble/gatt_vars.c"
                            "nimble/hid_func.c"
                            "paw3395/paw3395.cpp"
                            "ui.cpp"
                            "oled/ssd1306.c"
                            "battery/battery.cpp"

                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio driver esp_driver_i2c esp_adc
                    INCLUDE_DIRS "." "paw3395" "nimble" "button" "oled" "battery" "timer")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
