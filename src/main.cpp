#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#include "dht22.h"
#include "sensor_data.h"

#define DHT22_PIN GPIO_NUM_4

#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_6

#define SENSOR_SAMPLE_PERIOD_MS 2000
#define SENSOR_TASK_STACK_SIZE 4096
#define SENSOR_TASK_PRIORITY 2
#define DATA_TASK_STACK_SIZE 4096
#define DATA_TASK_PRIORITY 1
#define ADC_RAW_MAX 4095
#define SENSOR_QUEUE_LENGTH 1

static const char *TAG = "sensor";

static adc_oneshot_unit_handle_t adc_handle;
static QueueHandle_t sensor_data_queue = nullptr;

static esp_err_t LdrInit()
{
    adc_oneshot_unit_init_cfg_t unit_config = {};

    unit_config.unit_id = LDR_ADC_UNIT;
    unit_config.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t result =
        adc_oneshot_new_unit(
            &unit_config,
            &adc_handle
        );

    if (result != ESP_OK)
    {
        return result;
    }

    adc_oneshot_chan_cfg_t channel_config = {};

    channel_config.atten = ADC_ATTEN_DB_12;
    channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;

    return adc_oneshot_config_channel(
        adc_handle,
        LDR_ADC_CHANNEL,
        &channel_config
    );
}

static int LdrReadPercent()
{
    int raw = 0;

    esp_err_t result =
        adc_oneshot_read(
            adc_handle,
            LDR_ADC_CHANNEL,
            &raw
        );

    if (result != ESP_OK)
    {
        return -1;
    }

    // Convert the raw ADC value to a relative 0-100% level.
    // Normalize the 12-bit ADC code; this is not a calibrated lux value.
    int percent =
        (raw * 100) / ADC_RAW_MAX;

    if (percent < 0)
    {
        percent = 0;
    }

    if (percent > 100)
    {
        percent = 100;
    }

    return percent;
}

// Wait for the latest complete snapshot and print it to the serial monitor.
static void SensorDataLogTask(void *argument)
{
    (void)argument;

    SensorData sample{};

    while (true)
    {
        if (xQueueReceive(
                sensor_data_queue,
                &sample,
                portMAX_DELAY) == pdPASS)
        {
            ESP_LOGI(TAG, "Temperature: %.1f C", sample.temperature);
            ESP_LOGI(TAG, "Humidity: %.1f %%", sample.humidity);
            ESP_LOGI(TAG, "LDR ADC level: %d %%", sample.lightLevel);
        }
    }
}

static void SensorTask(void *argument)
{
    (void)argument;

    TickType_t last_wake_time =
        xTaskGetTickCount();

    while (true)
    {
        // Collect readings into the data object that Part V will queue.
        SensorData sample{};
        DHT22Data dht_data{};
        bool sample_is_valid = true;

        /*
         * Read DHT22.
         */
        esp_err_t dht_result =
            dht22_read(&dht_data);

        if (dht_result == ESP_OK)
        {
            sample.temperature = dht_data.temperature;
            sample.humidity = dht_data.humidity;

        }
        else
        {
            sample_is_valid = false;
            ESP_LOGE(
                TAG,
                "DHT22 read failed: %s",
                esp_err_to_name(dht_result)
            );
        }

        /*
         * Read LDR.
         */
        int light_level =
            LdrReadPercent();

        if (light_level >= 0)
        {
            sample.lightLevel = light_level;

        }
        else
        {
            sample_is_valid = false;
            ESP_LOGE(
                TAG,
                "LDR read failed"
            );
        }

        // motionDetected stays false until the PIR sensor is added.
        if (sample_is_valid)
        {
            // This one-item queue keeps the latest sample if its reader is late.
            xQueueOverwrite(sensor_data_queue, &sample);
        }

        /*
         * Maintain a stable 2-second period.
         */
        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(
                SENSOR_SAMPLE_PERIOD_MS
            )
        );
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(
        TAG,
        "BCA152 FreeRTOS Multisensor"
    );

    ESP_LOGI(
        TAG,
        "System starting..."
    );

    /*
     * Initialize DHT22.
     */
    ESP_LOGI(
        TAG,
        "Initializing DHT22..."
    );

    esp_err_t result =
        dht22_init(DHT22_PIN);

    if (result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "DHT22 initialization failed: %s",
            esp_err_to_name(result)
        );

        return;
    }

    ESP_LOGI(
        TAG,
        "DHT22 initialized successfully."
    );

    /*
     * Initialize LDR.
     */
    ESP_LOGI(
        TAG,
        "Initializing LDR..."
    );

    result = LdrInit();

    if (result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "LDR initialization failed: %s",
            esp_err_to_name(result)
        );

        return;
    }

    ESP_LOGI(
        TAG,
        "LDR initialized successfully."
    );

    // One slot is sufficient when consumers only need the latest sensor data.
    sensor_data_queue = xQueueCreate(
        SENSOR_QUEUE_LENGTH,
        sizeof(SensorData)
    );

    if (sensor_data_queue == nullptr)
    {
        ESP_LOGE(TAG, "Could not create sensor data queue");
        return;
    }

    BaseType_t consumer_result = xTaskCreate(
        SensorDataLogTask,
        "SensorDataLogTask",
        DATA_TASK_STACK_SIZE,
        nullptr,
        DATA_TASK_PRIORITY,
        nullptr
    );

    if (consumer_result != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create SensorDataLogTask");
        vQueueDelete(sensor_data_queue);
        sensor_data_queue = nullptr;
        return;
    }

    BaseType_t task_result =
        xTaskCreate(
            SensorTask,
            "SensorTask",
            SENSOR_TASK_STACK_SIZE,
            nullptr,
            SENSOR_TASK_PRIORITY,
            nullptr
        );

    if (task_result != pdPASS)
    {
        ESP_LOGE(
            TAG,
            "Could not create SensorTask"
        );
    }
}
