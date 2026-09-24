#include "alarm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "rtos_objects.h"

namespace
{
constexpr char TAG[] = "sensor";
}

// Observe the alarm state bit; this task can own buzzer control when one is added.
void AlarmTask(void *argument)
{
    (void)argument;
    bool alarm_was_active = false;

    while (true)
    {
        EventBits_t event_bits = xEventGroupGetBits(system_events);
        bool alarm_is_active = (event_bits & EVENT_ALARM) != 0;

        if (alarm_is_active != alarm_was_active)
        {
            if (alarm_is_active)
            {
                SERIAL_LOGW(TAG, "Temperature alarm condition active");
            }
            else
            {
                SERIAL_LOGI(TAG, "Temperature alarm condition cleared");
            }
            alarm_was_active = alarm_is_active;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
