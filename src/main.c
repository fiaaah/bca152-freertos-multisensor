#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void SensorTask(void *pvParameters)
{
    while (1)
    {
        printf("SensorTask: reading sensors...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void DisplayTask(void *pvParameters)
{
    while (1)
    {
        printf("DisplayTask: updating display...\n");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main(void)
{
    printf("BCA152 FreeRTOS Multisensor\n");
    printf("System starting...\n");

    xTaskCreate(
        SensorTask,
        "SensorTask",
        2048,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        DisplayTask,
        "DisplayTask",
        2048,
        NULL,
        1,
        NULL
    );
}