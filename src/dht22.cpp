#include "dht22.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"

#define DHT22_START_TIME_US 20000
#define DHT22_TIMEOUT_US    100
#define DHT22_SAMPLE_US     1
#define DHT22_BIT_SAMPLE_US 40

static gpio_num_t dht22_pin;

static portMUX_TYPE dht22_mux =
    portMUX_INITIALIZER_UNLOCKED;

static int wait_for_level(
    int target_level,
    uint32_t timeout_us)
{
    uint32_t elapsed = 0;

    while (gpio_get_level(dht22_pin) != target_level)
    {
        if (elapsed >= timeout_us)
        {
            return -1;
        }

        esp_rom_delay_us(DHT22_SAMPLE_US);
        elapsed += DHT22_SAMPLE_US;
    }

    return elapsed;
}

esp_err_t dht22_init(gpio_num_t pin)
{
    dht22_pin = pin;

    gpio_config_t config = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t result = gpio_config(&config);

    if (result != ESP_OK)
    {
        return result;
    }

    return gpio_set_level(dht22_pin, 1);
}

esp_err_t dht22_read(DHT22Data *data)
{
    if (data == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[5] = {0};
    esp_err_t result;

    /*
     * Send the DHT22 start signal.
     */
    result = gpio_set_direction(
        dht22_pin,
        GPIO_MODE_OUTPUT_OD
    );
    if (result != ESP_OK)
    {
        return result;
    }

    result = gpio_set_level(
        dht22_pin,
        0
    );
    if (result != ESP_OK)
    {
        return result;
    }

    esp_rom_delay_us(
        DHT22_START_TIME_US
    );

    result = gpio_set_level(
        dht22_pin,
        1
    );
    if (result != ESP_OK)
    {
        return result;
    }

    result = gpio_set_direction(
        dht22_pin,
        GPIO_MODE_INPUT
    );
    if (result != ESP_OK)
    {
        return result;
    }

    /*
     * Wait for the DHT22 response.
     */
    if (wait_for_level(0, DHT22_TIMEOUT_US) < 0)
    {
        return ESP_ERR_TIMEOUT;
    }

    if (wait_for_level(1, DHT22_TIMEOUT_US) < 0)
    {
        return ESP_ERR_TIMEOUT;
    }

    if (wait_for_level(0, DHT22_TIMEOUT_US) < 0)
    {
        return ESP_ERR_TIMEOUT;
    }

    /*
     * Read all 40 data bits.
     */
    portENTER_CRITICAL(&dht22_mux);

    for (int bit = 0; bit < 40; bit++)
    {
        if (wait_for_level(1, DHT22_TIMEOUT_US) < 0)
        {
            portEXIT_CRITICAL(&dht22_mux);
            return ESP_ERR_TIMEOUT;
        }

        esp_rom_delay_us(DHT22_BIT_SAMPLE_US);

        int level = gpio_get_level(dht22_pin);

        if (level == 1)
        {
            int byte_index = bit / 8;
            int bit_index = 7 - (bit % 8);

            bytes[byte_index] |=
                (1U << bit_index);
        }

        if (wait_for_level(0, DHT22_TIMEOUT_US) < 0)
        {
            portEXIT_CRITICAL(&dht22_mux);
            return ESP_ERR_TIMEOUT;
        }
    }

    portEXIT_CRITICAL(&dht22_mux);

    /*
     * Check the checksum.
     */
    uint8_t checksum =
        bytes[0] +
        bytes[1] +
        bytes[2] +
        bytes[3];

    if (checksum != bytes[4])
    {
        return ESP_ERR_INVALID_CRC;
    }

    /*
     * Convert humidity.
     */
    uint16_t raw_humidity =
        ((uint16_t)bytes[0] << 8) |
        bytes[1];

    data->humidity =
        raw_humidity / 10.0f;

    /*
     * Convert temperature.
     */
    uint16_t raw_temperature =
        ((uint16_t)bytes[2] << 8) |
        bytes[3];

    if (raw_temperature & 0x8000)
    {
        raw_temperature &= 0x7FFF;

        data->temperature =
            -(raw_temperature / 10.0f);
    }
    else
    {
        data->temperature =
            raw_temperature / 10.0f;
    }

    return ESP_OK;
}
