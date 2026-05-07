#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <math.h>
#include "keypad.h"
#include "Ch7SegDisplay.h"

static void compassTask(void);
static void qmc5883lTask(void);

#define CALIBRATION_SECONDS 30
#define RESET_SECONDS 5

K_THREAD_DEFINE(compassThread, 1024, compassTask, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(qmc5883lThread, 1024, qmc5883lTask, NULL, NULL, NULL, 7, 0, 0);
K_SEM_DEFINE(qmc5883lSem, 0, 1);
K_SEM_DEFINE(qmc5883lCalibrate, 0, 1);
K_SEM_DEFINE(qmc5883lCalibrateReset, 0, 1);
K_SEM_DEFINE(qmc5883lCalibrateCancel, 0, 1);

#define COMPASS_UPDATE_INTERVAL K_MSEC(250)

static atomic_t whole_heading = ATOMIC_INIT(0);

static struct {
    struct {
        float x, y;
    } offset, scale;
} calibration, calibrationDefault = {
    .offset = {201.5f, -946.5f},
    .scale = {1.01f, 0.99f}
};

static void compassTask(void) {
    k_thread_name_set(compassThread, "compass");

    bool printValues = false;
    bool displayOn = false;

    while (1) {
        uint32_t events = k_event_wait(&compassKeyEvent, (KEY_EVT_COMPASS_ON | KEY_EVT_COMPASS_OFF | KEY_EVT_COMPASS_CALIBRATE | KEY_EVT_COMPASS_RESET_START), true, COMPASS_UPDATE_INTERVAL);

        // Check if display is used by a different task
        isSegmentDisplayOn(&displayOn, K_FOREVER);
        if (!printValues && displayOn) {
            continue;
        }

        if (events & KEY_EVT_COMPASS_ON) {
            printValues = true;
            setSegmentDisplayFormat(DISPLAY_DIGIT_ALWAYS_ALL, K_FOREVER);
            setSegmentDisplayOn(K_FOREVER);

        } else if (events & KEY_EVT_COMPASS_OFF) {
            printValues = false;
            setSegmentDisplayValue(0, K_FOREVER);
            setSegmentDisplayFormat(0, K_FOREVER);
            setSegmentDisplayOff(K_FOREVER);
        } else if (events & KEY_EVT_COMPASS_CALIBRATE) {
            k_sem_give(&qmc5883lCalibrate);
            setSegmentDisplayFormat(DISPLAY_FORMAT_NONE, K_FOREVER);
            setSegmentDisplayOn(K_FOREVER);

            uint16_t i;
            for (i = CALIBRATION_SECONDS; i > 0; i--) {
                setSegmentDisplayValue(i, K_FOREVER);
                uint32_t event_end = k_event_wait(&compassKeyEvent, (KEY_EVT_COMPASS_CALIBRATE), true, K_SECONDS(1));
                if (event_end & KEY_EVT_COMPASS_CALIBRATE) {
                    k_sem_give(&qmc5883lCalibrateCancel);
                    break;
                }
            }

            if (i == 0) {
                k_sem_give(&qmc5883lCalibrate);
            }

            setSegmentDisplayValue(0, K_FOREVER);
            setSegmentDisplayFormat(0, K_FOREVER);
            setSegmentDisplayOff(K_FOREVER);

        } else if (events & KEY_EVT_COMPASS_RESET_START) {
            setSegmentDisplayFormat(DISPLAY_FORMAT_NONE, K_FOREVER);
            setSegmentDisplayOn(K_FOREVER);

            uint16_t i;
            for (i = RESET_SECONDS; i > 0; i--) {
                setSegmentDisplayValue(i, K_FOREVER);
                uint32_t event_end = k_event_wait(&compassKeyEvent, (KEY_EVT_COMPASS_RESET_STOP), true, K_SECONDS(1));
                if (event_end & KEY_EVT_COMPASS_RESET_STOP) {
                    break;
                }
            }

            if (i == 0) {
                setSegmentDisplayFormat(DISPLAY_DIGIT_ALWAYS_ALL, K_FOREVER);
                setSegmentDisplayValue(0, K_FOREVER);

                uint32_t event_end = k_event_wait(&compassKeyEvent, (KEY_EVT_COMPASS_RESET_STOP), true, K_FOREVER);
                if (event_end & KEY_EVT_COMPASS_RESET_STOP) {
                    k_sem_give(&qmc5883lCalibrateReset);
                }
            }

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

static void qmc5883lTask(void) {
    k_thread_name_set(qmc5883lThread, "qmc5883l");

    const struct gpio_dt_spec interruptPin = GPIO_DT_SPEC_GET(DT_ALIAS(compass), int_gpios);
    const struct i2c_dt_spec qmc5883l = I2C_DT_SPEC_GET(DT_ALIAS(compass));

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

    calibration = calibrationDefault;

    while (1) {
        if (k_sem_take(&qmc5883lCalibrateReset, K_NO_WAIT) == 0) {
            calibration = calibrationDefault;
        }

        if (k_sem_take(&qmc5883lCalibrate, K_NO_WAIT) == 0) {
            int16_t min_x = 32767, max_x = -32768;
            int16_t min_y = 32767, max_y = -32768;
            
            bool calibrate;

            while (true) {
                k_sem_take(&qmc5883lSem, K_FOREVER);
                
                if (i2c_burst_read_dt(&qmc5883l, 0x00, buffer, 6) == 0) {
                    int16_t temp_x = (int16_t)((uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8));
                    int16_t temp_y = (int16_t)((uint16_t)buffer[2] | ((uint16_t)buffer[3] << 8));

                    if (temp_x < min_x) {
                        min_x = temp_x;
                    }

                    if (temp_x > max_x) {
                        max_x = temp_x;
                    }

                    if (temp_y < min_y) {
                        min_y = temp_y;
                    }

                    if (temp_y > max_y) {
                        max_y = temp_y;
                    }
                }

                if (k_sem_take(&qmc5883lCalibrate, K_NO_WAIT) == 0) {
                    calibrate = true;
                    break;
                }

                if (k_sem_take(&qmc5883lCalibrateCancel, K_NO_WAIT) == 0) {
                    calibrate = false;
                    break;
                }
            }

            if (calibrate) {
                calibration.offset.x = (max_x + min_x) / 2.0f;
                calibration.offset.y = (max_y + min_y) / 2.0f;
                
                float xDelta = (max_x - min_x) / 2.0f;
                float yDelta = (max_y - min_y) / 2.0f;
                float avgDelta = (xDelta + yDelta) / 2.0f;
                
                if (xDelta != 0) {
                    calibration.scale.x = avgDelta / xDelta;
                }

                if (yDelta != 0) {
                    calibration.scale.y = avgDelta / yDelta;
                }
            }
        }

        k_sem_take(&qmc5883lSem, K_FOREVER);
        if (i2c_burst_read_dt(&qmc5883l, 0x00, buffer, 6) == 0) {
            int16_t x = (int16_t)((uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8));
            int16_t y = (int16_t)((uint16_t)buffer[2] | ((uint16_t)buffer[3] << 8));

            float xCalibrated = ((float)x - calibration.offset.x) * calibration.scale.x;
            float yCalibrated = ((float)y - calibration.offset.y) * calibration.scale.y;

            float heading_rad = atan2f(-xCalibrated, yCalibrated);
            float heading_deg = heading_rad * (180.0f / 3.14159265f);

            if (heading_deg < 0.0f) {
                heading_deg += 360.0f;
            }

            atomic_set(&whole_heading, (int)heading_deg);
        }
    }
}
