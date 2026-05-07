#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <errno.h>
#include "keypad.h"
#include "Ch7SegDisplay.h"

static void batteryTask(void);

K_THREAD_DEFINE(batteryThread, 1024, batteryTask, NULL, NULL, NULL, 10, 0, 0);

/* Ni-Mh battery HF9 (min 7.0 V) */
#define LOW_BATTERY_THRESHOLD_VOLTS_1 7
#define LOW_BATTERY_THRESHOLD_VOLTS_2 500000

#define BATTERY_UPDATE_INTERVAL K_SECONDS(10)

static void batteryTask(void) {
    k_thread_name_set(batteryThread, "battery");

    const struct gpio_dt_spec batteryLed = GPIO_DT_SPEC_GET(DT_ALIAS(battery_led), gpios);
    const struct device *batteryADC = DEVICE_DT_GET(DT_ALIAS(battery_adc));

    __ASSERT(gpio_is_ready_dt(&batteryLed), "Battery LED GPIO not ready!");
    __ASSERT(device_is_ready(batteryADC), "Battery ADC not ready!");

    gpio_pin_configure_dt(&batteryLed, GPIO_OUTPUT_INACTIVE);

    struct sensor_value voltage;

    bool printValues = false;
    bool displayOn = false;

    while (1) {
        uint32_t events = k_event_wait(&batteryKeyEvent, (KEY_EVT_BATTERY_ON | KEY_EVT_BATTERY_OFF), true, BATTERY_UPDATE_INTERVAL);

        if (sensor_sample_fetch(batteryADC) == 0) {
            sensor_channel_get(batteryADC, SENSOR_CHAN_VOLTAGE, &voltage);

            if (voltage.val1 < LOW_BATTERY_THRESHOLD_VOLTS_1 || (voltage.val1 == LOW_BATTERY_THRESHOLD_VOLTS_1 && voltage.val2 <= LOW_BATTERY_THRESHOLD_VOLTS_2)) {
                gpio_pin_set_dt(&batteryLed, 1);
            } else {
                gpio_pin_set_dt(&batteryLed, 0);
            }
        }

        // Check if disp[lay is used by a different task
        isSegmentDisplayOn(&displayOn, K_FOREVER);
        if (!printValues && displayOn) {
            continue;
        }

        if (events & KEY_EVT_BATTERY_ON) {
            printValues = true;
            setSegmentDisplayFormat(DISPLAY_DIGIT_ALWAYS_MIDDLE | DISPLAY_DOT_RIGHT, K_FOREVER);
            setSegmentDisplayOn(K_FOREVER);
        } else if (events & KEY_EVT_BATTERY_OFF) {
            printValues = false;
            setSegmentDisplayValue(0, K_FOREVER);
            setSegmentDisplayFormat(0, K_FOREVER);
            setSegmentDisplayOff(K_FOREVER);
        }

        if (printValues) {
            setSegmentDisplayValue(voltage.val1 % 100 * 10 + voltage.val2 / 100000, K_FOREVER);
        }
    }
}
