# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS esp_ads1256.c
        INCLUDE_DIRS include
        REQUIRES esp_system esp_timer log esp_driver_spi esp_driver_gpio esp_hal_gpio freertos
    )
else()
    set(EXTRA_COMPONENT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/example
    )

    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(esp_ads1256)
endif()