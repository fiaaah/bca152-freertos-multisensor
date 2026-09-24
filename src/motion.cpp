#include "motion.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "rtos_objects.h"
#include "system_state.h"

namespace
{
constexpr gpio_num_t PIR_PIN = GPIO_NUM_27;
constexpr int MOTION_INACTIVITY_TIMEOUT_MS = 15000;
constexpr int MOTION_TASK_POLL_MS = 100;
constexpr char TAG[] = "sensor";
}

void MotionTask(void *argument)
{
    (void)argument;

    gpio_config_t pir_config = {};
    pir_config.pin_bit_mask = (1ULL << PIR_PIN);
    pir_config.mode = GPIO_MODE_INPUT;
    pir_config.pull_up_en = GPIO_PULLUP_DISABLE;
    pir_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    pir_config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t result = gpio_config(&pir_config);
    if (result != ESP_OK)
    {
        SERIAL_LOGE(TAG, "PIR GPIO setup failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    MotionState motion_state = MotionState::INACTIVE;
    TickType_t last_motion_tick = 0;
    while (true)
    {
        TickType_t now = xTaskGetTickCount();
        int pir_level = gpio_get_level(PIR_PIN);
        bool motion_detected = (pir_level == 1);

        if (motion_detected)
        {
            last_motion_tick = now;
            xEventGroupSetBits(system_events, EVENT_MOTION);
        }
        else
        {
            xEventGroupClearBits(system_events, EVENT_MOTION);
        }

        MotionState next_state = evaluateSystemState(
            motion_state,
            motion_detected,
            pdTICKS_TO_MS(now - last_motion_tick),
            MOTION_INACTIVITY_TIMEOUT_MS
        );

        if (next_state != motion_state)
        {
            motion_state = next_state;
            if (motion_state == MotionState::ACTIVE)
            {
                xEventGroupSetBits(system_events, EVENT_ACTIVE);
                SERIAL_LOGI(TAG, "Motion detected");
            }
            else
            {
                xEventGroupClearBits(system_events, EVENT_ACTIVE);
                SERIAL_LOGI(TAG, "Motion inactive after timeout");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MOTION_TASK_POLL_MS));
    }
}
