#include "compass.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>
#include "keypad.h"
#include "Ch7SegDisplay.h"

static void compassTask(void);
static void qmc5883lTask(void);

K_THREAD_DEFINE(compassThread, 1024, compassTask, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(qmc5883lThread, 1024, qmc5883lTask, NULL, NULL, NULL, 7, 0, 0);
K_SEM_DEFINE(qmc5883lSem, 0, 1);

#define COMPASS_UPDATE_INTERVAL K_MSEC(100)

static atomic_t whole_heading = ATOMIC_INIT(0);

static void compassTask(void) {
    k_thread_name_set(compassThread, "compass");

    bool printValues = false;
    bool displayOn = false;

    while (1) {
        uint32_t events = k_event_wait(&compassKeyEvent, (KEY_EVT_COMPASS_ON | KEY_EVT_COMPASS_OFF), true, COMPASS_UPDATE_INTERVAL);

        // Check if disp[lay is used by a different task
        isSegmentDisplayOn(&displayOn, K_FOREVER);
        if (!printValues && displayOn) {
            continue;
        }

        if (events & KEY_EVT_COMPASS_ON) {
            printValues = true;
            setSegmentDisplayFormat(DISPLAY_DIGIT_ALWAYS_ALL | DISPLAY_DOT_NONE, K_FOREVER);
            setSegmentDisplayOn(K_FOREVER);
        } else if (events & KEY_EVT_COMPASS_OFF) {
            printValues = false;
            setSegmentDisplayValue(0, K_FOREVER);
            setSegmentDisplayFormat(0, K_FOREVER);
            setSegmentDisplayOff(K_FOREVER);
        }

        if (printValues) {
            setSegmentDisplayValue(atomic_get(&whole_heading), K_FOREVER);
        }
    }
}

static struct gpio_callback interruptPinCbData;

void interruptPinCallback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_sem_give(&qmc5883lSem);
}

#define QMC5883L DT_ALIAS(compass)
#define INTERRUPT DT_PATH(zephyr_user)

static void qmc5883lTask(void) {
    k_thread_name_set(qmc5883lThread, "qmc5883l");

    const struct gpio_dt_spec interruptPin = GPIO_DT_SPEC_GET(INTERRUPT, int_gpios);
    const struct i2c_dt_spec qmc5883l = I2C_DT_SPEC_GET(QMC5883L);

    __ASSERT(gpio_is_ready_dt(&interruptPin), "Interrupt GPIO not ready!");
    __ASSERT(i2c_is_ready_dt(&qmc5883l), "QMC5883L I2C not ready!");

    gpio_pin_configure_dt(&interruptPin, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&interruptPin, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&interruptPinCbData, interruptPinCallback, BIT(interruptPin.pin));
    gpio_add_callback(interruptPin.port, &interruptPinCbData);

    i2c_reg_write_byte_dt(&qmc5883l, 0x0a, 0x80);
    i2c_reg_write_byte_dt(&qmc5883l, 0x0b, 0x01);
    i2c_reg_write_byte_dt(&qmc5883l, 0x09, 0x01);

    uint8_t buffer[6];

    while (1) {
        k_sem_take(&qmc5883lSem, K_FOREVER);
        if (i2c_burst_read_dt(&qmc5883l, 0x00, buffer, 6) == 0) {
            int16_t x = (int16_t)((uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8));
            int16_t y = (int16_t)((uint16_t)buffer[2] | ((uint16_t)buffer[3] << 8));

            float heading_rad = atan2f(y, x);
            float heading_deg = heading_rad * (180.0f / 3.14159265f);

            if (heading_deg < 0.0f) {
                heading_deg += 360.0f;
            }

            atomic_set(&whole_heading, (int)heading_deg);
        }
    }
}
