#include <zephyr/drivers/sensor.h>
#include <math.h>
#include "altimeter.h"

static void altimeterTask(void);

#define UPDATE_INTERVAL_MS 1000

K_THREAD_DEFINE(altimeterThread, 1024, altimeterTask, NULL, NULL, NULL, 7, 0, 0);
K_MUTEX_DEFINE(altimeterMutex);
K_MUTEX_DEFINE(returnAltimeterMutex);

static altimeter_data_t returnData;

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

static inline void adjustAltitude(const struct sensor_value *altitude, const struct sensor_value *dieTemp, struct sensor_value *adjustedAltitude) {
    float dieTemp_C = dieTemp->val1 + dieTemp->val2 / 1000000.0f;
    float altitude_m = altitude->val1 + altitude->val2 / 1000000.0f;

    altitude_m *= (dieTemp_C + 273.15f) / 288.15f;

    adjustedAltitude->val1 = (int32_t)altitude_m;
    adjustedAltitude->val2 = (int32_t)((altitude_m - adjustedAltitude->val1) * 1000000);
}

static void altimeterTask(void) {
    k_thread_name_set(altimeterThread, "altimeter");

    const struct device *altimeter = DEVICE_DT_GET(DT_ALIAS(altimeter));

    __ASSERT(device_is_ready(altimeter), "Altimeter not ready!");

    int64_t interval = k_ms_to_ticks_near64(UPDATE_INTERVAL_MS);
    int64_t next_wake_time = k_uptime_ticks();

    struct sensor_value pressure;
    struct sensor_value dieTemp;
    struct sensor_value altitude;
    struct sensor_value adjAltitude;

    while (1) {
        k_sleep(K_TIMEOUT_ABS_TICKS(next_wake_time));
        next_wake_time = k_uptime_ticks() + interval;

        if (sensor_sample_fetch(altimeter) == 0) {
            sensor_channel_get(altimeter, SENSOR_CHAN_PRESS, &pressure);
            sensor_channel_get(altimeter, SENSOR_CHAN_DIE_TEMP, &dieTemp);
            
            calculateAltitude(&pressure, &altitude);
            adjustAltitude(&altitude, &dieTemp, &adjAltitude);

            altimeter_data_t newReturnData = {
                .altitude = altitude.val1,
                .adjAltitude = adjAltitude.val1,
                .environment = {
                    .pressure = (pressure.val1 + (pressure.val2 / 1000000.0f)) * 100.0f,
                    .temperature = dieTemp.val1 + (dieTemp.val2 / 1000000.0f),
                },
            };

            if (k_mutex_lock(&returnAltimeterMutex, K_TIMEOUT_ABS_TICKS(next_wake_time)) == 0) {
                returnData = newReturnData;
                k_mutex_unlock(&returnAltimeterMutex);
            }
        }
    }
}
