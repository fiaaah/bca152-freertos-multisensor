#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "alarm.h"
#include "display.h"
#include "input.h"
#include "motion.h"
#include "rtos_objects.h"
#include "sensor_data.h"
#include "sensors.h"
#include "system_state.h"

namespace
{
constexpr char TAG[] = "sensor";
constexpr UBaseType_t SENSOR_QUEUE_LENGTH = 1;
constexpr UBaseType_t DISPLAY_MODE_QUEUE_LENGTH = 1;

// Stack sizes and priorities follow the lab's suggested starting schedule.
constexpr uint32_t SENSOR_TASK_STACK_SIZE = 4096;
constexpr UBaseType_t SENSOR_TASK_PRIORITY = 2;
constexpr uint32_t DISPLAY_TASK_STACK_SIZE = 4096;
constexpr UBaseType_t DISPLAY_TASK_PRIORITY = 1;
constexpr uint32_t INPUT_TASK_STACK_SIZE = 3072;
constexpr UBaseType_t INPUT_TASK_PRIORITY = 3;
constexpr uint32_t MOTION_TASK_STACK_SIZE = 3072;
constexpr UBaseType_t MOTION_TASK_PRIORITY = 3;
constexpr uint32_t ALARM_TASK_STACK_SIZE = 3072;
constexpr UBaseType_t ALARM_TASK_PRIORITY = 2;

bool CreateTask(TaskFunction_t task, const char *name, uint32_t stack_size,
                UBaseType_t priority)
{
    BaseType_t result = xTaskCreate(task, name, stack_size, nullptr, priority, nullptr);
    if (result != pdPASS)
    {
        SERIAL_LOGE(TAG, "Could not create %s", name);
        return false;
    }
    return true;
}
} // namespace

// Startup only: initialize hardware/RTOS objects, then create the application tasks.
extern "C" void app_main(void)
{
    serial_mutex = xSemaphoreCreateMutex();
    if (serial_mutex == nullptr)
    {
        ESP_LOGE(TAG, "Could not create serial output mutex");
        return;
    }

    SERIAL_LOGI(TAG, "BCA152 FreeRTOS Multisensor");
    SERIAL_LOGI(TAG, "System starting...");
    SERIAL_LOGI(TAG, "Initializing sensors...");

    esp_err_t result = SensorsInit();
    if (result != ESP_OK)
    {
        SERIAL_LOGE(TAG, "Sensor initialization failed: %s", esp_err_to_name(result));
        return;
    }
    SERIAL_LOGI(TAG, "Sensors initialized successfully.");

    result = AlarmInit();
    if (result != ESP_OK)
    {
        SERIAL_LOGE(TAG, "Buzzer initialization failed: %s", esp_err_to_name(result));
        return;
    }
    SERIAL_LOGI(TAG, "Buzzer initialized successfully.");

    sensor_data_queue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorData));
    if (sensor_data_queue == nullptr)
    {
        SERIAL_LOGE(TAG, "Could not create sensor data queue");
        return;
    }

    // InputTask publishes the latest selection; DisplayTask consumes it and owns OLED writes.
    display_mode_queue = xQueueCreate(DISPLAY_MODE_QUEUE_LENGTH, sizeof(DisplayMode));
    if (display_mode_queue == nullptr)
    {
        SERIAL_LOGE(TAG, "Could not create display mode queue");
        vQueueDelete(sensor_data_queue);
        sensor_data_queue = nullptr;
        return;
    }

    system_events = xEventGroupCreate();
    if (system_events == nullptr)
    {
        SERIAL_LOGE(TAG, "Could not create system event group");
        vQueueDelete(sensor_data_queue);
        vQueueDelete(display_mode_queue);
        sensor_data_queue = nullptr;
        display_mode_queue = nullptr;
        return;
    }

    // Each worker owns its own operation after startup completes.
    if (!CreateTask(DisplayTask, "DisplayTask", DISPLAY_TASK_STACK_SIZE, DISPLAY_TASK_PRIORITY) ||
        !CreateTask(SensorTask, "SensorTask", SENSOR_TASK_STACK_SIZE, SENSOR_TASK_PRIORITY) ||
        !CreateTask(InputTask, "InputTask", INPUT_TASK_STACK_SIZE, INPUT_TASK_PRIORITY) ||
        !CreateTask(MotionTask, "MotionTask", MOTION_TASK_STACK_SIZE, MOTION_TASK_PRIORITY) ||
        !CreateTask(AlarmTask, "AlarmTask", ALARM_TASK_STACK_SIZE, ALARM_TASK_PRIORITY))
    {
        SERIAL_LOGE(TAG, "Task startup failed");
    }
}
