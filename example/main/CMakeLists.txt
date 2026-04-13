idf_component_register(SRCS main.c
                            header.c
                        INCLUDE_DIRS "."
                        REQUIRES esp_ads1256 esp_driver_gpio esp_hal_gpio esp_driver_spi esp_timer log
)
