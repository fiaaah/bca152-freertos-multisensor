#ifndef DHT22_H
#define DHT22_H

#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief Read temperature and humidity from a DHT22 sensor.
 *
 * @param pin GPIO pin connected to DHT22 DATA.
 * @param temperature Pointer where temperature will be stored.
 * @param humidity Pointer where humidity will be stored.
 *
 * @return ESP_OK if reading is valid, otherwise ESP_FAIL.
 */
esp_err_t dht22_read(gpio_num_t pin, float *temperature, float *humidity);

// Returns the stage where the most recent read failed, for serial diagnostics.
const char *dht22_error_stage(void);

#endif
