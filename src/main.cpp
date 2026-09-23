#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#include "dht22.h"

#define DHT22_PIN GPIO_NUM_4

#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_6

#define SENSOR_SAMPLE_PERIOD_MS 2000
#define SENSOR_TASK_STACK_SIZE 4096
#define SENSOR_TASK_PRIORITY 2
#define ADC_RAW_MAX 4095

static const char *TAG = "sensor";

static adc_oneshot_unit_handle_t adc_handle;

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

static void SensorTask(void *argument)
{
    (void)argument;

    TickType_t last_wake_time =
        xTaskGetTickCount();

    while (true)
    {
        DHT22Data dht_data;

        /*
         * Read DHT22.
         */
        esp_err_t dht_result =
            dht22_read(&dht_data);

        if (dht_result == ESP_OK)
        {
            ESP_LOGI(
                TAG,
                "Temperature: %.1f C",
                dht_data.temperature
            );

            ESP_LOGI(
                TAG,
                "Humidity: %.1f %%",
                dht_data.humidity
            );
        }
        else
        {
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
            ESP_LOGI(
                TAG,
                "LDR ADC level: %d %%",
                light_level
            );
        }
        else
        {
            ESP_LOGE(
                TAG,
                "LDR read failed"
            );
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
