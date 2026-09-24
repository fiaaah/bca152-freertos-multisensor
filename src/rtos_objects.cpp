#include "rtos_objects.h"

QueueHandle_t sensor_data_queue = nullptr;
EventGroupHandle_t system_events = nullptr;
SemaphoreHandle_t serial_mutex = nullptr;
