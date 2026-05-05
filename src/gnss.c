#include <zephyr/drivers/gnss.h>
#include "gnss.h"

static void gnssTask(void);

K_THREAD_DEFINE(gnssThread, 1024, gnssTask, NULL, NULL, NULL, 7, 0, 0);
K_MUTEX_DEFINE(gnssDataMutex);
K_MUTEX_DEFINE(returnGnssMutex);
K_SEM_DEFINE(gnssDataSem, 0, 1);

static volatile struct gnss_data gnssData;
atomic_t gnssSatellitesCount = ATOMIC_INIT(0);

static gnss_data_t returnData;

int gnssGetData(gnss_data_t *data, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&returnGnssMutex, timeout))) {
        return ret;
    }

    *data = returnData;

    k_mutex_unlock(&returnGnssMutex);
    return 0;
}

static void gnssDataCb(const struct device *dev, const struct gnss_data *data) {
    if (k_mutex_lock(&gnssDataMutex, K_NO_WAIT)) {
        return;
    }

    gnssData = *data;

    k_mutex_unlock(&gnssDataMutex);
    k_sem_give(&gnssDataSem);
}

static void gnssSatellitesCb(const struct device *dev, const struct gnss_satellite *satellites, uint16_t size) {
    atomic_set(&gnssSatellitesCount, size);
}

GNSS_DT_DATA_CALLBACK_DEFINE(DT_ALIAS(gnss), gnssDataCb);
GNSS_DT_SATELLITES_CALLBACK_DEFINE(DT_ALIAS(gnss), gnssSatellitesCb);

static void gnssTask(void) {
    k_thread_name_set(gnssThread, "gnss");

    struct gnss_data data;

    while (1) {
        k_sem_take(&gnssDataSem, K_FOREVER);

        k_mutex_lock(&gnssDataMutex, K_FOREVER);
        data = gnssData;
        k_mutex_unlock(&gnssDataMutex);

        const char *fix_status_str;
        const char *fix_quality_str;

        switch (data.info.fix_status) {
            case GNSS_FIX_STATUS_NO_FIX:        fix_status_str = "No Fix"; break;
            case GNSS_FIX_STATUS_GNSS_FIX:      fix_status_str = "GNSS Fix"; break;
            case GNSS_FIX_STATUS_DGNSS_FIX:     fix_status_str = "DGNSS Fix"; break;
            case GNSS_FIX_STATUS_ESTIMATED_FIX: fix_status_str = "Estimated"; break;
            default:                            fix_status_str = "Unknown"; break;
        }

        switch (data.info.fix_quality) {
            case GNSS_FIX_QUALITY_INVALID:   fix_quality_str = "Invalid"; break;
            case GNSS_FIX_QUALITY_GNSS_SPS:  fix_quality_str = "GNSS SPS"; break;
            case GNSS_FIX_QUALITY_DGNSS:     fix_quality_str = "DGNSS"; break;
            case GNSS_FIX_QUALITY_GNSS_PPS:  fix_quality_str = "GNSS PPS"; break;
            case GNSS_FIX_QUALITY_RTK:       fix_quality_str = "RTK"; break;
            case GNSS_FIX_QUALITY_FLOAT_RTK: fix_quality_str = "Float RTK"; break;
            case GNSS_FIX_QUALITY_ESTIMATED: fix_quality_str = "Estimated"; break;
            default:                         fix_quality_str = "Unknown"; break;
        }

        gnss_data_t newReturnData = {
            .datetime = {
                .time = {
                    .hour = data.utc.hour,
                    .minute = data.utc.minute,
                    .second = data.utc.millisecond / 1000
                },
                .date = {
                    .day = data.utc.month_day,
                    .month = data.utc.month,
                    .year = data.utc.century_year
                }
            },
            .position = {
                .latitude = data.nav_data.latitude / 1e9,
                .longitude = data.nav_data.longitude / 1e9
            },
            .altitude = data.nav_data.altitude / 1000,
            .heading = data.nav_data.bearing / 1000,
            .speed = (float)data.nav_data.speed / 1000.0f,
            .gnss = {
                .satellites = atomic_get(&gnssSatellitesCount)
            }
        };

        strcpy(newReturnData.gnss.fixStatus, fix_status_str);
        strcpy(newReturnData.gnss.quality, fix_quality_str);

        k_mutex_lock(&returnGnssMutex, K_FOREVER);
        returnData = newReturnData;
        k_mutex_unlock(&returnGnssMutex);
    }
}
    