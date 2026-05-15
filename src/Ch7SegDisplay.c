#include "Ch7SegDisplay.h"
#include <zephyr/drivers/gpio.h>

#include "SystemStats.h"

static void segmentDisplayTask(void);

K_THREAD_DEFINE(segmentDisplayThread, 1024, segmentDisplayTask, NULL, NULL, NULL, 1, 0, 0);
K_MUTEX_DEFINE(segmentDisplayDataMutex);
K_SEM_DEFINE(segmentDisplaySem, 0, 1);

static volatile bool display = 0;
static volatile uint16_t val = 0;
static volatile uint8_t frm = 0;

int setSegmentDisplayOn(k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&segmentDisplayDataMutex, timeout))) {
        return ret;
    }

    display = 1;
    k_mutex_unlock(&segmentDisplayDataMutex);
    k_sem_give(&segmentDisplaySem);
    return 0;
}

int setSegmentDisplayOff(k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&segmentDisplayDataMutex, timeout))) {
        return ret;
    }

    display = 0;
    k_mutex_unlock(&segmentDisplayDataMutex);
    return 0;
}

int isSegmentDisplayOn(bool *val, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&segmentDisplayDataMutex, timeout))) {
        return ret;
    }

    *val = display;
    k_mutex_unlock(&segmentDisplayDataMutex);
    return 0;
}

int setSegmentDisplayValue(uint16_t value, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&segmentDisplayDataMutex, timeout))) {
        return ret;
    }
    
    val = value;
    k_mutex_unlock(&segmentDisplayDataMutex);
    return 0;
}

int setSegmentDisplayFormat(uint8_t format, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&segmentDisplayDataMutex, timeout))) {
        return ret;
    }

    frm = format;
    k_mutex_unlock(&segmentDisplayDataMutex);
    return 0;
}

static const struct gpio_dt_spec displayPins[] = {
    DT_FOREACH_PROP_ELEM_SEP(DT_ALIAS(segment_display), gpios, GPIO_DT_SPEC_GET_BY_IDX, (,))
};

#define SEGMENT_TIME_US 500
#define SEGMENT_MISS_US 100

#define SEGMENT_NONE 24
#define NR_SEGMENTS 23

#define DIGIT_0_OFFSET 14
#define DIGIT_1_OFFSET 7
#define DIGIT_2_OFFSET 0

#define DOT_0_OFFSET 22
#define DOT_1_OFFSET 21

#define DIGIT_NONE 10

#define DOT_ON 1
#define DOT_OFF 0

static const struct segmentConfig {
    uint8_t high;
    uint8_t low;
} segmentMap[NR_SEGMENTS] = {
    {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {1, 2}, {1, 3},
    {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {1, 4}, {1, 5},
    {2, 1}, {3, 1}, {4, 1}, {5, 1}, {3, 2}, {4, 2}, {5, 2},
    {2, 3}, {2, 4}
};

static const uint8_t digitSegments[11] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0100111, // 7
    0b1111111, // 8
    0b1101111, // 9
    0b0000000
};

static inline void setSegment(uint8_t segment) {
    for (size_t i = 0; i < ARRAY_SIZE(displayPins); i++) {
        gpio_pin_configure_dt(&displayPins[i], GPIO_DISCONNECTED);
    }

    if (segment > NR_SEGMENTS) {
        return;
    }

    gpio_pin_configure_dt(&displayPins[segmentMap[segment].high], GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&displayPins[segmentMap[segment].low], GPIO_OUTPUT_INACTIVE);
}

static inline uint32_t toSegments(uint8_t d2, uint8_t s1, uint8_t d1, uint8_t s0, uint8_t d0) {
    return s0 << DOT_0_OFFSET | s1 << DOT_1_OFFSET | digitSegments[d0] << DIGIT_0_OFFSET | digitSegments[d1] << DIGIT_1_OFFSET | digitSegments[d2] << DIGIT_2_OFFSET;
}

static uint32_t getSegments(k_timeout_t timeout) {
    static uint32_t segments = 0;
    static uint8_t format = 0;
    static uint16_t value = 0;

    if (!k_mutex_lock(&segmentDisplayDataMutex, timeout)) {
        if (!display) {
            k_mutex_unlock(&segmentDisplayDataMutex);
            return 0;
        }

        if (value != val || format != frm) {
            value = val;
            format = frm;
        } else {
            k_mutex_unlock(&segmentDisplayDataMutex);
            return segments;
        }

        k_mutex_unlock(&segmentDisplayDataMutex);

    } else {
        return segments;
    }

    uint8_t d0 = value % 10;
    uint8_t d1 = (value / 10) % 10;
    uint8_t d2 = (value / 100) % 10;

    switch (format & 0b1100) {
        case DISPLAY_DIGIT_ALWAYS_NONE:
            d1 = (d1 || d2) ? d1 : DIGIT_NONE;
            /* no break */

        case DISPLAY_DIGIT_ALWAYS_MIDDLE:
            d2 = d2 ? d2 : DIGIT_NONE;
            /* no break */

        case DISPLAY_DIGIT_ALWAYS_ALL:
            break;
    }

    switch (format & 0b11) {
        case DISPLAY_DOT_NONE:
            segments = toSegments(d2, DOT_OFF, d1, DOT_OFF, d0);
            break;

        case DISPLAY_DOT_RIGHT:
            segments = toSegments(d2, DOT_OFF, d1, DOT_ON, d0);
            break;

        case DISPLAY_DOT_LEFT:
            segments = toSegments(d2, DOT_ON, d1, DOT_OFF, d0);
            break;

        case DISPLAY_DOT_BOTH:
            segments = toSegments(d2, DOT_ON, d1, DOT_ON, d0);
            break;
    }

    return segments;
}

atomic_t segmentDisplayMissCount = ATOMIC_INIT(0);
atomic_t segmentDisplayLatencyUs = ATOMIC_INIT(0);
atomic_t segmentDisplayJitterUs = ATOMIC_INIT(0);

static void segmentDisplayTask(void) {
    k_thread_name_set(segmentDisplayThread, "segmentDisplay");

    for (size_t i = 0; i < ARRAY_SIZE(displayPins); i++) {
        if (gpio_is_ready_dt(&displayPins[i])) {
            gpio_pin_configure_dt(&displayPins[i], GPIO_INPUT); 
        } else {
            printk("Display GPIO %zu not ready!\n", i);
            k_panic();
        }
    }

    int64_t interval = k_us_to_ticks_near64(SEGMENT_TIME_US);
    int64_t next_wake_time = 0;
    uint32_t segments = 0;

    #if PRINT_STATS == 1
    int64_t expected_wake_time = 0;
    uint64_t drift_current_us = 0;
    uint64_t drift_min_us = INT64_MAX;
    uint64_t drift_max_us = 0;
    uint64_t drift_us = 0;
    #endif /* PRINT_STATS */

    while (1) {
        if (!segments) {
            setSegment(SEGMENT_NONE);
            k_sem_take(&segmentDisplaySem, K_FOREVER);
            segments = getSegments(K_FOREVER);
            next_wake_time = k_uptime_ticks() + interval;
        } else {
            for (uint8_t i = 0; i < NR_SEGMENTS; i++) {
                segments = getSegments(K_TIMEOUT_ABS_TICKS(next_wake_time));

                if (segments & BIT(i)) {
                    setSegment(i);
                } else {
                    setSegment(SEGMENT_NONE);
                }

                #if PRINT_STATS == 1
                drift_min_us = MIN(drift_min_us, drift_current_us);
                drift_max_us = MAX(drift_max_us, drift_current_us);
                drift_us = MAX(atomic_get(&segmentDisplayLatencyUs), drift_current_us);

                if (drift_current_us > SEGMENT_MISS_US) {
                    atomic_inc(&segmentDisplayMissCount);
                }

                atomic_set(&segmentDisplayLatencyUs, drift_us);
                atomic_set(&segmentDisplayJitterUs, drift_max_us - drift_min_us);

                expected_wake_time = next_wake_time;

                #endif /* PRINT_STATS */

                k_sleep(K_TIMEOUT_ABS_TICKS(next_wake_time));

                #if PRINT_STATS == 1
                drift_current_us = k_ticks_to_us_near64(k_uptime_ticks() - expected_wake_time);
                #endif /* PRINT_STATS */
                
                next_wake_time += interval;
            }
        } 
    }
}
