# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(
    SRCS
        "main.cpp"
        "dht22.cpp"
        "alarm_logic.cpp"
        "sensors.cpp"
        "display.cpp"
        "input.cpp"
        "alarm.cpp"
        "motion.cpp"
        "system_state.cpp"
        "rtos_objects.cpp"
    INCLUDE_DIRS
        "../include"
    REQUIRES
        ssd1306
        esp_driver_i2c
        esp_driver_ledc
)
