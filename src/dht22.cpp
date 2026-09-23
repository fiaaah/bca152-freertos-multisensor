#include "dht22.h"

#include <stdint.h> // Fixed-width fields used in the DHT22 data frame.

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define DHT_TIMEOUT_US 200

static const char *s_error_stage = "not started";

const char *dht22_error_stage(void)
{
    return s_error_stage;
}

static esp_err_t wait_for_level(gpio_num_t pin, int level, int *duration_us)
{
    int64_t start_us = esp_timer_get_time();

    // Stop waiting if the expected signal edge does not arrive in time.
    while (gpio_get_level(pin) != level) {
        if ((esp_timer_get_time() - start_us) > DHT_TIMEOUT_US) {
            return ESP_ERR_TIMEOUT;
        }
    }

    *duration_us = (int)(esp_timer_get_time() - start_us);
    return ESP_OK;
}

esp_err_t dht22_read(
    gpio_num_t pin,
    float *temperature,
    float *humidity)
{
    // The sensor sends 40 bits: humidity, temperature, then checksum.
    uint8_t data[5] = {0, 0, 0, 0, 0};
    int pulse_us = 0;
    esp_err_t err;

    if (temperature == NULL || humidity == NULL) {
        s_error_stage = "invalid argument";
        return ESP_ERR_INVALID_ARG;
    }

    s_error_stage = "configuring output";
    /*
     * Send start signal.
     * Pull DATA low for at least 1 ms.
     */
    // Open-drain lets the ESP32 pull the shared data wire low or release it.
    err = gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    if (err != ESP_OK) {
        return err;
    }

    err = gpio_set_level(pin, 0);
    if (err != ESP_OK) {
        return err;
    }

    esp_rom_delay_us(1200);

    /*
     * Release the bus.
     */
    // In open-drain mode, setting 1 releases the wire for the sensor to drive.
    err = gpio_set_level(pin, 1);
    if (err != ESP_OK) {
        return err;
    }

    /*
     * DHT22 response:
     * LOW ~80 us
     * HIGH ~80 us
     */
    s_error_stage = "waiting for response LOW";
    err = wait_for_level(pin, 0, &pulse_us);
    if (err != ESP_OK) {
        return err;
    }

    s_error_stage = "waiting for response HIGH";
    err = wait_for_level(pin, 1, &pulse_us);
    if (err != ESP_OK) {
        return err;
    }

    s_error_stage = "waiting for first data LOW";
    err = wait_for_level(pin, 0, &pulse_us);
    if (err != ESP_OK) {
        return err;
    }

    /*
     * Read 40 bits.
     */
    for (int bit = 0; bit < 40; bit++) {

        /*
         * Each bit begins with a LOW pulse.
         */
        s_error_stage = "waiting for data bit LOW";
        err = wait_for_level(pin, 0, &pulse_us);
        if (err != ESP_OK) {
            return err;
        }

        /*
         * Measure HIGH pulse.
         *
         * Short HIGH = 0
         * Long HIGH  = 1
         */
        int high_time_us = 0;
        s_error_stage = "measuring data bit HIGH";
        err = wait_for_level(pin, 1, &pulse_us);
        if (err != ESP_OK) {
            return err;
        }

        err = wait_for_level(pin, 0, &high_time_us);
        if (err != ESP_OK) {
            return err;
        }

        // Shift in the next bit; a longer HIGH pulse represents binary 1.
        data[bit / 8] <<= 1;

        if (high_time_us > 40) {
            data[bit / 8] |= 1;
        }
    }

    /*
     * Checksum verification.
     */
    // Reject incomplete or corrupted readings before returning measurements.
    uint8_t checksum =
        (uint8_t)(data[0] +
                  data[1] +
                  data[2] +
                  data[3]);

    if (checksum != data[4]) {
        s_error_stage = "checksum mismatch";
        return ESP_FAIL;
    }

    /*
     * Humidity:
     * first 16 bits / 10
     */
    uint16_t raw_humidity =
        ((uint16_t)data[0] << 8) |
        data[1];

    *humidity = raw_humidity / 10.0f;

    /*
     * Temperature:
     * next 16 bits / 10
     */
    uint16_t raw_temperature =
        ((uint16_t)(data[2] & 0x7F) << 8) |
        data[3];

    *temperature = raw_temperature / 10.0f;

    /*
     * Bit 15 indicates negative temperature.
     */
    if (data[2] & 0x80) {
        *temperature = -*temperature;
    }

    s_error_stage = "success";
    return ESP_OK;
}
