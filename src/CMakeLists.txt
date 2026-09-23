# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(
    SRCS
        "main.cpp"
        "dht22.cpp"
        "alarm_logic.cpp"
    INCLUDE_DIRS
        "../include"
    REQUIRES
        chill-sam__ssd1306
        esp_driver_i2c
)
