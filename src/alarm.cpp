#include "alarm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#include "rtos_objects.h"

namespace
{
constexpr char TAG[] = "sensor";
constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_26;
constexpr ledc_mode_t BUZZER_SPEED_MODE = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t BUZZER_TIMER = LEDC_TIMER_0;
constexpr ledc_channel_t BUZZER_CHANNEL = LEDC_CHANNEL_0;
constexpr uint32_t BUZZER_FREQUENCY_HZ = 2000;
constexpr uint32_t BUZZER_DUTY_ON = 512; // 50% duty for 10-bit PWM

esp_err_t SetBuzzer(bool enabled)
{
    const uint32_t duty = enabled ? BUZZER_DUTY_ON : 0;
    esp_err_t result = ledc_set_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL, duty);
    if (result != ESP_OK)
    {
        return result;
    }
    return ledc_update_duty(BUZZER_SPEED_MODE, BUZZER_CHANNEL);
}
}

esp_err_t AlarmInit()
{
    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = BUZZER_SPEED_MODE;
    timer_config.duty_resolution = LEDC_TIMER_10_BIT;
    timer_config.timer_num = BUZZER_TIMER;
    timer_config.freq_hz = BUZZER_FREQUENCY_HZ;
    timer_config.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t result = ledc_timer_config(&timer_config);
    if (result != ESP_OK)
    {
        return result;
    }

    ledc_channel_config_t channel_config = {};
    channel_config.gpio_num = BUZZER_PIN;
    channel_config.speed_mode = BUZZER_SPEED_MODE;
    channel_config.channel = BUZZER_CHANNEL;
    channel_config.intr_type = LEDC_INTR_DISABLE;
    channel_config.timer_sel = BUZZER_TIMER;
    channel_config.duty = 0;
    channel_config.hpoint = 0;
    return ledc_channel_config(&channel_config);
}

// AlarmTask owns the buzzer output; sensor logic only publishes the alarm event bit.
void AlarmTask(void *argument)
{
    (void)argument;
    bool alarm_was_active = false;

    while (true)
    {
        EventBits_t event_bits = xEventGroupGetBits(system_events);
        bool alarm_is_active = (event_bits & EVENT_ALARM) != 0;

        if (alarm_is_active != alarm_was_active)
        {
            esp_err_t buzzer_result = SetBuzzer(alarm_is_active);
            if (buzzer_result != ESP_OK)
            {
                SERIAL_LOGE(TAG, "Buzzer control failed: %s", esp_err_to_name(buzzer_result));
            }

            if (alarm_is_active)
            {
                SERIAL_LOGW(TAG, "Temperature alarm condition active");
            }
            else
            {
                SERIAL_LOGI(TAG, "Temperature alarm condition cleared");
            }
            alarm_was_active = alarm_is_active;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
