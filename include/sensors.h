#pragma once

#include "esp_err.h"

esp_err_t SensorsInit();
void SensorTask(void *argument);
