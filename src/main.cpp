#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "dht22.h"

#define DHT22_PIN GPIO_NUM_4
#define SENSOR_SAMPLE_PERIOD_MS 2000
#define SENSOR_TASK_STACK_SIZE 4096
#define SENSOR_TASK_PRIORITY 2

static const char *TAG = "dht22_test";

static void SensorTask(void *argument)
{
    (void)argument;

    TickType_t last_wake_time = xTaskGetTickCount();
    float temperature = 0.0f;
    float humidity = 0.0f;

    while (true) {
        esp_err_t result = dht22_read(DHT22_PIN, &temperature, &humidity);

        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Temperature: %.1f C", temperature);
            ESP_LOGI(TAG, "Humidity: %.1f %%", humidity);
        } else {
            ESP_LOGE(TAG, "DHT22 read failed: %s (%s)",
                     esp_err_to_name(result), dht22_error_stage());
        }

        vTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "BCA152 FreeRTOS Multisensor");
    ESP_LOGI(TAG, "System starting...");
    ESP_LOGI(TAG, "Part IV - DHT22 Test");

    BaseType_t result = xTaskCreate(
        SensorTask,
        "SensorTask",
        SENSOR_TASK_STACK_SIZE,
        nullptr,
        SENSOR_TASK_PRIORITY,
        nullptr);

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Could not create SensorTask");
    }
}
