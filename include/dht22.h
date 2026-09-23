#ifndef DHT22_H
#define DHT22_H

#include "esp_err.h"
#include "driver/gpio.h"

struct DHT22Data
{
    float temperature;
    float humidity;
};

esp_err_t dht22_init(gpio_num_t pin);
esp_err_t dht22_read(DHT22Data *data);

#endif