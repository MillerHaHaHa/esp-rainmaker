set(priv_req json_generator esp_rainmaker user_debug_header)

idf_component_register(SRCS "app_reset.c"
                    INCLUDE_DIRS "."
                    REQUIRES gpio_button rmaker_common
                    PRIV_REQUIRES ${priv_req})
