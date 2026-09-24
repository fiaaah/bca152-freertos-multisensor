#include "input.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "rtos_objects.h"
#include "system_state.h"

namespace
{
constexpr gpio_num_t ENCODER_CLK_PIN = GPIO_NUM_32;
constexpr gpio_num_t ENCODER_DT_PIN = GPIO_NUM_33;
constexpr char TAG[] = "sensor";

DisplayMode current_display_mode = DisplayMode::TEMPERATURE;
portMUX_TYPE display_mode_mux = portMUX_INITIALIZER_UNLOCKED;
}

void InputTask(void *argument)
{
    (void)argument;

    gpio_config_t encoder_config = {};
    encoder_config.pin_bit_mask = (1ULL << ENCODER_CLK_PIN) | (1ULL << ENCODER_DT_PIN);
    encoder_config.mode = GPIO_MODE_INPUT;
    encoder_config.pull_up_en = GPIO_PULLUP_ENABLE;
    encoder_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    encoder_config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t result = gpio_config(&encoder_config);
    if (result != ESP_OK)
    {
        SERIAL_LOGE(TAG, "Encoder GPIO setup failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    int previous_clk = gpio_get_level(ENCODER_CLK_PIN);
    while (true)
    {
        int current_clk = gpio_get_level(ENCODER_CLK_PIN);
        if (previous_clk == 1 && current_clk == 0)
        {
            bool clockwise = (gpio_get_level(ENCODER_DT_PIN) == 1);
            DisplayMode selected_mode;
            portENTER_CRITICAL(&display_mode_mux);
            current_display_mode = clockwise
                ? nextDisplayMode(current_display_mode)
                : previousDisplayMode(current_display_mode);
            selected_mode = current_display_mode;
            portEXIT_CRITICAL(&display_mode_mux);

            xQueueOverwrite(display_mode_queue, &selected_mode);
            SERIAL_LOGI(TAG, "Encoder page: %s", DisplayModeName(selected_mode));
        }

        previous_clk = current_clk;
        vTaskDelay(1);
    }
}
