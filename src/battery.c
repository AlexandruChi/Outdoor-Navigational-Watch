#include "battery.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <errno.h>
#include "keypad.h"
#include "Ch7SegDisplay.h"

static void batteryMonitorTask(void);

K_THREAD_DEFINE(batteryMonitorThread, 1024, batteryMonitorTask, NULL, NULL, NULL, 7, 0, 0);
K_EVENT_DEFINE(batteryEvents);

static const struct gpio_dt_spec batteryLed = GPIO_DT_SPEC_GET(DT_ALIAS(battery_led), gpios);
static const struct device *batteryADC = DEVICE_DT_GET(DT_ALIAS(battery_adc));
static const struct device *dieADC = DEVICE_DT_GET(DT_ALIAS(die_temp0));

#define LOW_BATTERY_THRESHOLD_VOLTS_1 7
#define LOW_BATTERY_THRESHOLD_VOLTS_2 700000

static void batteryMonitorTask(void) {
    k_thread_name_set(batteryMonitorThread, "batteryMonitor");

    struct sensor_value voltage;
    struct sensor_value dieTemp;

    bool printValues = false;

    __ASSERT(gpio_is_ready_dt(&batteryLed), "Battery LED GPIO not ready!");
    __ASSERT(device_is_ready(batteryADC), "Battery ADC not ready!");
    __ASSERT(device_is_ready(dieADC), "Die Temperature sensor not ready!");

    gpio_pin_configure_dt(&batteryLed, GPIO_OUTPUT_INACTIVE);

    while (1) {
        uint32_t events = k_event_wait(&batteryKeyEvent, (KEY_EVT_BATTERY_ON | KEY_EVT_BATTERY_OFF), true, K_SECONDS(1));

        if (events & KEY_EVT_BATTERY_ON) {
            printf("Battery key received hold event\n");

            printValues = setSegmentDisplayOn(K_MSEC(100));
            setSegmentDisplayFormat(DISPLAY_DOT_RIGHT);
        } else if (events & KEY_EVT_BATTERY_OFF) {
            printf("Battery key received release event\n");

            setSegmentDisplayValue(0);
            setSegmentDisplayFormat(DISPLAY_DOT_NONE);
            setSegmentDisplayOff();
            printValues = false;
        }

        if (sensor_sample_fetch(batteryADC) == 0) {
            sensor_channel_get(batteryADC, SENSOR_CHAN_VOLTAGE, &voltage);

            // printk("Battery voltage: %d.%06d V\n", voltage.val1, voltage.val2);

            if (voltage.val1 <= LOW_BATTERY_THRESHOLD_VOLTS_1 && voltage.val2 <= LOW_BATTERY_THRESHOLD_VOLTS_2) {
                k_event_post(&batteryEvents, BAT_EVT_LOW);
                gpio_pin_set_dt(&batteryLed, 1);
            } else {
                k_event_post(&batteryEvents, BAT_EVT_OK);
                gpio_pin_set_dt(&batteryLed, 0);
            }
        }

        if (printValues) {
            setSegmentDisplayValue(voltage.val1 % 100 * 10 + voltage.val2 / 100000);
        }

        // if (sensor_sample_fetch(dieADC) == 0) {
        //     sensor_channel_get(dieADC, SENSOR_CHAN_DIE_TEMP, &dieTemp);
            
        //     printk("Internal Temp: %d.%06d C\n", dieTemp.val1, dieTemp.val2);
        // }
    }
}
