#include "keypad.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <errno.h>
#include "Ch7SegDisplay.h"

static void keypadTask(void);

K_THREAD_DEFINE(keypadThread, 1024, keypadTask, NULL, NULL, NULL, 3, 0, 0);
K_SEM_DEFINE(keypadSem, 0, 1);

K_EVENT_DEFINE(batteryKeyEvent);
K_EVENT_DEFINE(navigationEvent);
K_EVENT_DEFINE(compassKeyEvent);

#define KEY_HOLD_TIME_MS 500

static const struct gpio_dt_spec batteryKey = GPIO_DT_SPEC_GET(DT_ALIAS(battery_key), gpios);
static const struct gpio_dt_spec leftKey = GPIO_DT_SPEC_GET(DT_ALIAS(left_key), gpios);
static const struct gpio_dt_spec rightKey = GPIO_DT_SPEC_GET(DT_ALIAS(right_key), gpios);
static const struct gpio_dt_spec actionKey = GPIO_DT_SPEC_GET(DT_ALIAS(compass_key), gpios);

static struct gpio_callback batteryKeyCbData;
static struct gpio_callback leftKeyCbData;
static struct gpio_callback rightKeyCbData;
static struct gpio_callback actionKeyCbData;

static atomic_t batteryKeyHold = ATOMIC_INIT(0);
static atomic_t leftKeyHold = ATOMIC_INIT(0);
static atomic_t rightKeyHold = ATOMIC_INIT(0);
static atomic_t actionKeyHold = ATOMIC_INIT(0);

static uint64_t batteryKeyTime;
static uint64_t leftKeyTime;
static uint64_t rightKeyTime;
static uint64_t actionKeyTime;

void batteryKeyCallback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    atomic_set(&batteryKeyHold, gpio_pin_get_dt(&batteryKey));
    k_sem_give(&keypadSem);
}

void leftKeyCallback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    atomic_set(&leftKeyHold, gpio_pin_get_dt(&leftKey));
    k_sem_give(&keypadSem);
}

void rightKeyCallback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    atomic_set(&rightKeyHold, gpio_pin_get_dt(&rightKey));
    k_sem_give(&keypadSem);
}

void actionKeyCallback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    atomic_set(&actionKeyHold, gpio_pin_get_dt(&actionKey));
    k_sem_give(&keypadSem);
}

#define STATE_RELEASE 0b00
#define STATE_PRESS 0b01
#define STATE_HELD 0b10

static void keypadTask(void) {
    k_thread_name_set(keypadThread, "keypad");

    if (!gpio_is_ready_dt(&batteryKey)) {
        printk("Battery Key GPIO not ready!\n");
        k_panic();
    }
    
    if (!gpio_is_ready_dt(&leftKey)) {
        printk("Left Key GPIO not ready!\n");
        k_panic();
    }

    if (!gpio_is_ready_dt(&rightKey)) {
        printk("Right Key GPIO not ready!\n");
        k_panic();
    }
    
    if (!gpio_is_ready_dt(&actionKey)) {
        printk("Action Key GPIO not ready!\n");
        k_panic();
    }

    gpio_pin_configure_dt(&batteryKey, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&batteryKey, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&batteryKeyCbData, batteryKeyCallback, BIT(batteryKey.pin));
    gpio_add_callback(batteryKey.port, &batteryKeyCbData);

    gpio_pin_configure_dt(&leftKey, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&leftKey, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&leftKeyCbData, leftKeyCallback, BIT(leftKey.pin));
    gpio_add_callback(leftKey.port, &leftKeyCbData);

    gpio_pin_configure_dt(&rightKey, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&rightKey, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&rightKeyCbData, rightKeyCallback, BIT(rightKey.pin));
    gpio_add_callback(rightKey.port, &rightKeyCbData);

    gpio_pin_configure_dt(&actionKey, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&actionKey, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&actionKeyCbData, actionKeyCallback, BIT(actionKey.pin));
    gpio_add_callback(actionKey.port, &actionKeyCbData);

    uint8_t batteryKeyState = STATE_RELEASE;
    uint8_t leftKeyState = STATE_RELEASE;
    uint8_t rightKeyState = STATE_RELEASE;
    uint8_t actionKeyState = STATE_RELEASE;

    while (1) {
        bool keyPressed = batteryKeyState | leftKeyState | rightKeyState | actionKeyState;
        keyPressed &= 0b01;

        k_sem_take(&keypadSem, keyPressed ? K_MSEC(10) : K_FOREVER);
        uint64_t now = k_uptime_get();
        
        bool bHold = atomic_get(&batteryKeyHold);
        bool lHold = atomic_get(&leftKeyHold);
        bool rHold = atomic_get(&rightKeyHold);
        bool aHold = atomic_get(&actionKeyHold);

        switch (batteryKeyState) {
            case STATE_RELEASE:
                if (bHold) {
                    batteryKeyState = STATE_PRESS;
                    batteryKeyTime = now;
                }
                break;

            case STATE_PRESS:
                if (now - batteryKeyTime >= KEY_HOLD_TIME_MS) {
                    batteryKeyState = STATE_HELD;
                    k_event_post(&batteryKeyEvent, KEY_EVT_BATTERY_ON);
                } else if (!bHold) {
                    batteryKeyState = STATE_RELEASE;
                    k_event_post(&navigationEvent, KEY_EVT_NAVIGATION_POWER);
                }
                break;

            case STATE_HELD:
                if (!bHold) {
                    batteryKeyState = STATE_RELEASE;
                    k_event_post(&batteryKeyEvent, KEY_EVT_BATTERY_OFF);
                }
                break;
        }

        switch (leftKeyState) {
            case STATE_RELEASE:
                if (lHold) {
                    leftKeyState = STATE_PRESS;
                    leftKeyTime = now;
                }
                break;

            case STATE_PRESS:
                if (now - leftKeyTime >= KEY_HOLD_TIME_MS) {
                    leftKeyState = STATE_HELD;
                    k_event_post(&compassKeyEvent, KEY_EVT_COMPASS_CALIBRATE);
                } else if (!lHold) {
                    leftKeyState = STATE_RELEASE;
                    k_event_post(&navigationEvent, KEY_EVT_NAVIGATION_LEFT);
                }
                break;

            case STATE_HELD:
                if (!lHold) {
                    leftKeyState = STATE_RELEASE;
                }
                break;
        }

        switch (rightKeyState) {
            case STATE_RELEASE:
                if (rHold) {
                    rightKeyState = STATE_PRESS;
                    rightKeyTime = now;
                }
                break;

            case STATE_PRESS:
                if (now - rightKeyTime >= KEY_HOLD_TIME_MS) {
                    rightKeyState = STATE_HELD;
                    k_event_post(&compassKeyEvent, KEY_EVT_COMPASS_RESET_START);
                } else if (!rHold) {
                    rightKeyState = STATE_RELEASE;
                    k_event_post(&navigationEvent, KEY_EVT_NAVIGATION_RIGHT);
                }
                break;

            case STATE_HELD:
                if (!rHold) {
                    rightKeyState = STATE_RELEASE;
                    k_event_post(&compassKeyEvent, KEY_EVT_COMPASS_RESET_STOP);
                }
                break;
        }

        switch (actionKeyState) {
            case STATE_RELEASE:
                if (aHold) {
                    actionKeyState = STATE_PRESS;
                    actionKeyTime = now;
                }
                break;

            case STATE_PRESS:
                if (now - actionKeyTime >= KEY_HOLD_TIME_MS) {
                    actionKeyState = STATE_HELD;
                    k_event_post(&compassKeyEvent, KEY_EVT_COMPASS);
                } else if (!aHold) {
                    actionKeyState = STATE_RELEASE;
                    k_event_post(&navigationEvent, KEY_EVT_NAVIGATION_SELECT);
                }
                break;

            case STATE_HELD:
                if (!aHold) {
                    actionKeyState = STATE_RELEASE;
                }
                break;
        }
    }
}
