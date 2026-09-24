#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Shared FreeRTOS objects are defined once in src/rtos_objects.cpp.
extern QueueHandle_t sensor_data_queue;
extern QueueHandle_t display_mode_queue;
extern EventGroupHandle_t system_events;
extern SemaphoreHandle_t serial_mutex;

// Event bits: ACTIVE is owned by MotionTask, MOTION by MotionTask, ALARM by SensorTask.
#define EVENT_ACTIVE BIT0
#define EVENT_MOTION BIT1
#define EVENT_ALARM  BIT2

// Keep each task's log message together on the shared serial output.
#define SERIAL_LOGI(tag, ...) \
    do { \
        xSemaphoreTake(serial_mutex, portMAX_DELAY); \
        ESP_LOGI(tag, __VA_ARGS__); \
        xSemaphoreGive(serial_mutex); \
    } while (0)

#define SERIAL_LOGW(tag, ...) \
    do { \
        xSemaphoreTake(serial_mutex, portMAX_DELAY); \
        ESP_LOGW(tag, __VA_ARGS__); \
        xSemaphoreGive(serial_mutex); \
    } while (0)

#define SERIAL_LOGE(tag, ...) \
    do { \
        xSemaphoreTake(serial_mutex, portMAX_DELAY); \
        ESP_LOGE(tag, __VA_ARGS__); \
        xSemaphoreGive(serial_mutex); \
    } while (0)
