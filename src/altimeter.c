#include <zephyr/drivers/sensor.h>
#include <math.h>
#include "altimeter.h"

#include "SystemStats.h"

static void altimeterTask(void);

#define ALTIMETER_UPDATE_INTERVAL K_MSEC(200)
#define ALTIMETER_MISS_US 100

K_THREAD_DEFINE(altimeterThread, 1024, altimeterTask, NULL, NULL, NULL, 7, 0, 0);
K_MUTEX_DEFINE(returnAltimeterMutex);

static altimeter_data_t returnData;

static struct sensor_value tempOffset = {
    .val1 = -2,
    .val2 = -500000,
};

int altimeterGetData(altimeter_data_t *data, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&returnAltimeterMutex, timeout))) {
        return ret;
    }

    *data = returnData;

    k_mutex_unlock(&returnAltimeterMutex);
    return 0;
}

static inline void calculateAltitude(const struct sensor_value *pressure, struct sensor_value *altitude) {
    float pressure_kPa = pressure->val1 + pressure->val2 / 1000000.0f;
    float seaLevelPressure_kPa = 101.325f;

    float altitude_m = 44330.0 * (1.0 - pow(pressure_kPa / seaLevelPressure_kPa, 1.0f / 5.255f));

    altitude->val1 = (int32_t)altitude_m;
    altitude->val2 = (int32_t)((altitude_m - altitude->val1) * 1000000);
}

static inline void adjustAltitude(const struct sensor_value *altitude, const struct sensor_value *temp, struct sensor_value *adjustedAltitude) {
    float temp_C = temp->val1 + temp->val2 / 1000000.0f;
    float altitude_m = altitude->val1 + altitude->val2 / 1000000.0f;

    altitude_m *= (temp_C + 273.15f) / 288.15f;

    adjustedAltitude->val1 = (int32_t)altitude_m;
    adjustedAltitude->val2 = (int32_t)((altitude_m - adjustedAltitude->val1) * 1000000);
}

static inline void calculateAbsoluteHumidity(const struct sensor_value *temp, const struct sensor_value *humidity, struct sensor_value *absHumidity) {
    float temp_C = temp->val1 + temp->val2 / 1000000.0f;
    float humidity_pct = humidity->val1 + humidity->val2 / 1000000.0f;

    float abs_hum_g_m3 = (13.2447f * expf((17.67f * temp_C) / (temp_C + 243.5f)) * humidity_pct) / (temp_C + 273.15f);

    absHumidity->val1 = (int32_t)abs_hum_g_m3;
    absHumidity->val2 = (int32_t)((abs_hum_g_m3 - absHumidity->val1) * 1000000.0f);
}

atomic_t altimeterMissCount = ATOMIC_INIT(0);
atomic_t altimeterLatencyUs = ATOMIC_INIT(0);
atomic_t altimeterJitterUs = ATOMIC_INIT(0);

static void altimeterTask(void) {
    k_thread_name_set(altimeterThread, "altimeter");

    const struct device *altimeter = DEVICE_DT_GET(DT_ALIAS(altimeter));

    if (!device_is_ready(altimeter)) {
        printk("Altimeter not ready!\n");
        k_panic();
    }

    int64_t interval = ALTIMETER_UPDATE_INTERVAL.ticks;
    int64_t next_wake_time = 0;

    #if PRINT_STATS == 1
    int64_t expected_wake_time = 0;
    uint64_t drift_current_us = 0;
    uint64_t drift_min_us = INT64_MAX;
    uint64_t drift_max_us = 0;
    uint64_t drift_us = 0;
    #endif /* PRINT_STATS */

    struct sensor_value pressure;
    struct sensor_value temp;
    struct sensor_value humidity;
    struct sensor_value absHumidity;
    struct sensor_value altitude;
    struct sensor_value adjAltitude;

    next_wake_time = k_uptime_ticks() + interval;

    while (1) {
        if (sensor_sample_fetch(altimeter) == 0) {
            sensor_channel_get(altimeter, SENSOR_CHAN_PRESS, &pressure);
            sensor_channel_get(altimeter, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            sensor_channel_get(altimeter, SENSOR_CHAN_HUMIDITY, &humidity);

            temp.val1 += tempOffset.val1;
            temp.val2 += tempOffset.val2;
            
            calculateAltitude(&pressure, &altitude);
            adjustAltitude(&altitude, &temp, &adjAltitude);
            calculateAbsoluteHumidity(&temp, &humidity, &absHumidity);

            altimeter_data_t newReturnData = {
                .altitude = altitude.val1,
                .adjAltitude = adjAltitude.val1,
                .environment = {
                    .pressure = (pressure.val1 + (pressure.val2 / 1000000.0f)) * 1000.0f,
                    .temperature = temp.val1 + (temp.val2 / 1000000.0f),
                    .humidity = humidity.val1 + (humidity.val2 / 1000000.0f),
                    .absHumidity = absHumidity.val1 + (absHumidity.val2 / 1000000.0f),
                },
            };

            if (k_mutex_lock(&returnAltimeterMutex, K_TIMEOUT_ABS_TICKS(next_wake_time)) == 0) {
                returnData = newReturnData;
                k_mutex_unlock(&returnAltimeterMutex);
            }
        }

        #if PRINT_STATS == 1
        drift_min_us = MIN(drift_min_us, drift_current_us);
        drift_max_us = MAX(drift_max_us, drift_current_us);
        drift_us = MAX(atomic_get(&altimeterLatencyUs), drift_current_us);

        if (drift_current_us > ALTIMETER_MISS_US) {
            atomic_inc(&altimeterMissCount);
        }

        atomic_set(&altimeterLatencyUs, drift_us);
        atomic_set(&altimeterJitterUs, drift_max_us - drift_min_us);

        expected_wake_time = next_wake_time;

        #endif /* PRINT_STATS */

        k_sleep(K_TIMEOUT_ABS_TICKS(next_wake_time));

        #if PRINT_STATS == 1
        drift_current_us = k_ticks_to_us_near64(k_uptime_ticks() - expected_wake_time);
        #endif /* PRINT_STATS */

        next_wake_time += interval;
    }
}
