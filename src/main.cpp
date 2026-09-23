#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#include "dht22.h"
#include "sensor_data.h"

#include "driver/i2c_master.h"
#include "ssd1306.h"

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

#define ENCODER_CLK_PIN GPIO_NUM_32
#define ENCODER_DT_PIN GPIO_NUM_33
#define INPUT_TASK_STACK_SIZE 3072
#define INPUT_TASK_PRIORITY 1

#define PIR_PIN GPIO_NUM_27
#define MOTION_INACTIVITY_TIMEOUT_MS 15000
#define MOTION_TASK_POLL_MS 100
#define MOTION_TASK_STACK_SIZE 3072
#define MOTION_TASK_PRIORITY 1

enum class DisplayMode
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

enum class MotionState
{
    ACTIVE,
    INACTIVE
};

static DisplayMode current_display_mode = DisplayMode::TEMPERATURE;
static bool motion_detected = false;
static portMUX_TYPE display_mode_mux = portMUX_INITIALIZER_UNLOCKED;

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
// This task is the only task that writes to the OLED.
static void DisplayTask(void *argument)
{
    (void)argument;

    // Set up the I2C bus connected to the OLED.
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
        ESP_LOGE(TAG, "I2C initialization failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    // Describe the OLED: 128x64 pixels, I2C address 0x3C.
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
        ESP_LOGE(TAG, "OLED initialization failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    // Show the required startup screen.
    ssd1306_clear(display);
    ssd1306_draw_text(display, 0, 0, "ROOM MONITOR", true);
    ssd1306_draw_text(display, 0, 16, "Temperature: 25.4 C", true);
    ssd1306_display(display);
    bool display_was_active = true;

    SensorData sample{};

    while (true)
    {
        // Wait until SensorTask puts a new snapshot in the queue.
        if (xQueueReceive(
                sensor_data_queue,
                &sample,
                portMAX_DELAY) == pdPASS)
        {
            bool system_active;

            portENTER_CRITICAL(&display_mode_mux);
            system_active = motion_detected;
            portEXIT_CRITICAL(&display_mode_mux);

            if (system_active)
            {
                char temperature_text[32];
                snprintf(
                    temperature_text,
                    sizeof(temperature_text),
                    "Temperature: %.1f C",
                    sample.temperature
                );

                // This task alone updates the OLED.
                ssd1306_clear(display);
                ssd1306_draw_text(display, 0, 0, "ROOM MONITOR", true);
                ssd1306_draw_text(display, 0, 16, temperature_text, true);
                ssd1306_display(display);

                display_was_active = true;
            }
            else if (display_was_active)
            {
                // Blank the screen once when the system becomes inactive.
                ssd1306_clear(display);
                ssd1306_display(display);
                display_was_active = false;
            }

            // Keep the serial readings while we transition from the logger.
            ESP_LOGI(TAG, "Temperature: %.1f C", sample.temperature);
            ESP_LOGI(TAG, "Humidity: %.1f %%", sample.humidity);
            ESP_LOGI(TAG, "LDR ADC level: %d %%", sample.lightLevel);
        }
    }
}

static DisplayMode NextDisplayMode(DisplayMode mode, bool clockwise)
{
    if (clockwise)
    {
        switch (mode)
        {
            case DisplayMode::TEMPERATURE: return DisplayMode::HUMIDITY;
            case DisplayMode::HUMIDITY:    return DisplayMode::LIGHT;
            case DisplayMode::LIGHT:       return DisplayMode::MOTION;
            case DisplayMode::MOTION:      return DisplayMode::TEMPERATURE;
        }
    }
    else
    {
        switch (mode)
        {
            case DisplayMode::TEMPERATURE: return DisplayMode::MOTION;
            case DisplayMode::HUMIDITY:    return DisplayMode::TEMPERATURE;
            case DisplayMode::LIGHT:       return DisplayMode::HUMIDITY;
            case DisplayMode::MOTION:      return DisplayMode::LIGHT;
        }
    }

    return DisplayMode::TEMPERATURE;
}

static const char *DisplayModeName(DisplayMode mode)
{
    switch (mode)
    {
        case DisplayMode::TEMPERATURE: return "Temperature";
        case DisplayMode::HUMIDITY:    return "Humidity";
        case DisplayMode::LIGHT:       return "Light";
        case DisplayMode::MOTION:      return "Motion";
    }

    return "Unknown";
}

static void InputTask(void *argument)
{
    (void)argument;

    gpio_config_t encoder_config = {};
    encoder_config.pin_bit_mask =
        (1ULL << ENCODER_CLK_PIN) | (1ULL << ENCODER_DT_PIN);
    encoder_config.mode = GPIO_MODE_INPUT;
    encoder_config.pull_up_en = GPIO_PULLUP_ENABLE;
    encoder_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    encoder_config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t result = gpio_config(&encoder_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "Encoder GPIO setup failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    int previous_clk = gpio_get_level(ENCODER_CLK_PIN);

    while (true)
    {
        int current_clk = gpio_get_level(ENCODER_CLK_PIN);

        // A HIGH-to-LOW CLK transition marks a turn; DT tells us its direction.
        if (previous_clk == 1 && current_clk == 0)
        {
            bool clockwise = (gpio_get_level(ENCODER_DT_PIN) == 1);
            DisplayMode selected_mode;

            portENTER_CRITICAL(&display_mode_mux);
            current_display_mode =
                NextDisplayMode(current_display_mode, clockwise);
            selected_mode = current_display_mode;
            portEXIT_CRITICAL(&display_mode_mux);

            ESP_LOGI(TAG, "Encoder page: %s", DisplayModeName(selected_mode));
        }

        previous_clk = current_clk;
        vTaskDelay(1);
    }
}

static void MotionTask(void *argument)
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
        ESP_LOGE(TAG, "PIR GPIO setup failed: %s", esp_err_to_name(result));
        vTaskDelete(nullptr);
        return;
    }

    MotionState motion_state = MotionState::INACTIVE;
    TickType_t last_motion_tick = 0;

    while (true)
    {
        TickType_t now = xTaskGetTickCount();

        if (gpio_get_level(PIR_PIN) == 1)
        {
            // The PIR is reporting motion, so restart the inactivity timer.
            last_motion_tick = now;

            if (motion_state == MotionState::INACTIVE)
            {
                motion_state = MotionState::ACTIVE;

                portENTER_CRITICAL(&display_mode_mux);
                motion_detected = true;
                portEXIT_CRITICAL(&display_mode_mux);

                ESP_LOGI(TAG, "Motion detected");
            }
        }
        else if (motion_state == MotionState::ACTIVE &&
                 (now - last_motion_tick) >=
                     pdMS_TO_TICKS(MOTION_INACTIVITY_TIMEOUT_MS))
        {
            motion_state = MotionState::INACTIVE;

            portENTER_CRITICAL(&display_mode_mux);
            motion_detected = false;
            portEXIT_CRITICAL(&display_mode_mux);

            ESP_LOGI(TAG, "Motion inactive after timeout");
        }

        vTaskDelay(pdMS_TO_TICKS(MOTION_TASK_POLL_MS));
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

        // Copy the latest PIR state into this sensor snapshot.
        if (sample_is_valid)
        {
            portENTER_CRITICAL(&display_mode_mux);
            sample.motionDetected = motion_detected;
            portEXIT_CRITICAL(&display_mode_mux);

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
        DisplayTask,
        "DisplayTask",
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

    BaseType_t input_result = xTaskCreate(
        InputTask,
        "InputTask",
        INPUT_TASK_STACK_SIZE,
        nullptr,
        INPUT_TASK_PRIORITY,
        nullptr
    );

    if (input_result != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create InputTask");
    }
    BaseType_t motion_result = xTaskCreate(
    MotionTask,
    "MotionTask",
    MOTION_TASK_STACK_SIZE,
    nullptr,
    MOTION_TASK_PRIORITY,
    nullptr
);

    if (motion_result != pdPASS)
{
    ESP_LOGE(TAG, "Could not create MotionTask");
}
}
