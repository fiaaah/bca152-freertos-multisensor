#include "sensors.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "alarm_logic.h"
#include "dht22.h"
#include "rtos_objects.h"
#include "sensor_data.h"

namespace
{
constexpr gpio_num_t DHT22_PIN = GPIO_NUM_4;
constexpr adc_unit_t LDR_ADC_UNIT = ADC_UNIT_1;
constexpr adc_channel_t LDR_ADC_CHANNEL = ADC_CHANNEL_6;
constexpr int ADC_RAW_MAX = 4095;
constexpr int SENSOR_SAMPLE_PERIOD_MS = 2000;
constexpr char TAG[] = "sensor";

adc_oneshot_unit_handle_t adc_handle = nullptr;

esp_err_t LdrInit()
{
    adc_oneshot_unit_init_cfg_t unit_config = {};
    unit_config.unit_id = LDR_ADC_UNIT;
    unit_config.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t result = adc_oneshot_new_unit(&unit_config, &adc_handle);
    if (result != ESP_OK)
    {
        return result;
    }

    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.atten = ADC_ATTEN_DB_12;
    channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
    return adc_oneshot_config_channel(adc_handle, LDR_ADC_CHANNEL, &channel_config);
}

int LdrReadPercent()
{
    int raw = 0;
    if (adc_oneshot_read(adc_handle, LDR_ADC_CHANNEL, &raw) != ESP_OK)
    {
        return -1;
    }

    // This is a normalized ADC percentage, not a calibrated lux measurement.
    int percent = (raw * 100) / ADC_RAW_MAX;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}
} // namespace

esp_err_t SensorsInit()
{
    esp_err_t result = dht22_init(DHT22_PIN);
    if (result != ESP_OK)
    {
        return result;
    }
    return LdrInit();
}

void SensorTask(void *argument)
{
    (void)argument;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true)
    {
        SensorData sample{};
        DHT22Data dht_data{};
        bool sample_is_valid = true;

        esp_err_t dht_result = dht22_read(&dht_data);
        if (dht_result == ESP_OK)
        {
            sample.temperature = dht_data.temperature;
            sample.humidity = dht_data.humidity;
            if (evaluateTemperature(sample.temperature) == AlarmState::NORMAL)
            {
                xEventGroupClearBits(system_events, EVENT_ALARM);
            }
            else
            {
                xEventGroupSetBits(system_events, EVENT_ALARM);
            }
        }
        else
        {
            sample_is_valid = false;
            SERIAL_LOGE(TAG, "DHT22 read failed: %s", esp_err_to_name(dht_result));
        }

        int light_level = LdrReadPercent();
        if (light_level >= 0)
        {
            sample.lightLevel = light_level;
        }
        else
        {
            sample_is_valid = false;
            SERIAL_LOGE(TAG, "LDR read failed");
        }

        if (sample_is_valid)
        {
            EventBits_t event_bits = xEventGroupGetBits(system_events);
            sample.motionDetected = (event_bits & EVENT_MOTION) != 0;
            xQueueOverwrite(sensor_data_queue, &sample);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS));
    }
}
