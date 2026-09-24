#include "display.h"

#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ssd1306.h"

#include "rtos_objects.h"
#include "sensor_data.h"
#include "system_state.h"

namespace
{
constexpr char TAG[] = "sensor";
}

void DisplayTask(void *argument)
{
    (void)argument;

    // The display task owns all OLED setup and writes.
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = GPIO_NUM_21;
    bus_config.scl_io_num = GPIO_NUM_22;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 0;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t i2c_bus = nullptr;
    esp_err_t result = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (result != ESP_OK)
    {
        SERIAL_LOGE(TAG, "I2C initialization failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    ssd1306_config_t display_config = {};
    display_config.bus = SSD1306_I2C;
    display_config.width = 128;
    display_config.height = 64;
    display_config.iface.i2c.port = I2C_NUM_0;
    display_config.iface.i2c.addr = 0x3C;
    display_config.iface.i2c.rst_gpio = GPIO_NUM_NC;

    ssd1306_handle_t display = nullptr;
    result = ssd1306_new_i2c(&display_config, &display);
    if (result != ESP_OK)
    {
        SERIAL_LOGE(TAG, "OLED initialization failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    ssd1306_clear(display);
    ssd1306_draw_text(display, 0, 0, "ROOM MONITOR", true);
    ssd1306_draw_text(display, 0, 16, "Waiting for data", true);
    ssd1306_display(display);
    bool display_was_active = true;
    bool have_sample = false;
    DisplayMode current_mode = DisplayMode::TEMPERATURE;

    SensorData sample{};
    while (true)
    {
        // Poll both queues so a page change appears without waiting for another sensor sample.
        bool sample_updated =
            xQueueReceive(sensor_data_queue, &sample, pdMS_TO_TICKS(100)) == pdPASS;
        if (sample_updated)
        {
            have_sample = true;
            SERIAL_LOGI(TAG, "Temperature: %.1f C", sample.temperature);
            SERIAL_LOGI(TAG, "Humidity: %.1f %%", sample.humidity);
            SERIAL_LOGI(TAG, "LDR ADC level: %d %%", sample.lightLevel);
        }

        DisplayMode requested_mode;
        bool mode_updated =
            xQueueReceive(display_mode_queue, &requested_mode, 0) == pdPASS;
        if (mode_updated)
        {
            current_mode = requested_mode;
        }

        EventBits_t event_bits = xEventGroupGetBits(system_events);
        bool system_active = (event_bits & EVENT_ACTIVE) != 0;

        if (system_active && (sample_updated || mode_updated || !display_was_active))
        {
            ssd1306_clear(display);
            ssd1306_draw_text(display, 0, 0, "ROOM MONITOR", true);

            if (!have_sample)
            {
                ssd1306_draw_text(display, 0, 16, "Waiting for data", true);
            }
            else
            {
                char value_text[32];
                switch (current_mode)
                {
                    case DisplayMode::TEMPERATURE:
                        snprintf(value_text, sizeof(value_text), "Temp: %.1f C", sample.temperature);
                        break;
                    case DisplayMode::HUMIDITY:
                        snprintf(value_text, sizeof(value_text), "Humidity: %.1f %%", sample.humidity);
                        break;
                    case DisplayMode::LIGHT:
                        snprintf(value_text, sizeof(value_text), "Light: %d %%", sample.lightLevel);
                        break;
                    case DisplayMode::MOTION:
                        snprintf(value_text, sizeof(value_text), "Motion: %s",
                                 sample.motionDetected ? "Detected" : "None");
                        break;
                    default:
                        snprintf(value_text, sizeof(value_text), "Unknown page");
                        break;
                }
                ssd1306_draw_text(display, 0, 16, value_text, true);
            }

            ssd1306_display(display);
        }
        else if (!system_active && display_was_active)
        {
            // Clear once on entry to INACTIVE; the task remains the OLED's only writer.
            ssd1306_clear(display);
            ssd1306_display(display);
        }

        display_was_active = system_active;
    }
}
