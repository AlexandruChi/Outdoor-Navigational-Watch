#include "gnss.h"

static void gnssTask(void);

K_THREAD_DEFINE(gnssThread, 1024, gnssTask, NULL, NULL, NULL, 7, 0, 0);
K_MUTEX_DEFINE(gnssDataMutex);
K_SEM_DEFINE(gnssDataSem, 0, 1);

// TODO: make a structure with the data that is actually displayed on the screen

static volatile struct gnss_data gnssData;
atomic_t gnssSatellitesCount = ATOMIC_INIT(0);

int getGnssUTC(struct gnss_time *time, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&gnssDataMutex, timeout))) {
        return ret;
    }

    *time = gnssData.utc;

    k_mutex_unlock(&gnssDataMutex);
    return 0;
}

int getGnssInfo(struct gnss_info *info, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&gnssDataMutex, timeout))) {
        return ret;
    }

    *info = gnssData.info;

    k_mutex_unlock(&gnssDataMutex);
    return 0;
}

int getGnssNavData(struct navigation_data *navData, k_timeout_t timeout) {
    int ret = 0;
    if ((ret = k_mutex_lock(&gnssDataMutex, timeout))) {
        return ret;
    }

    *navData = gnssData.nav_data;

    k_mutex_unlock(&gnssDataMutex);
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

    // printf("%s reported %u satellites!\n", dev->name, size);
    // for (int i = 0; i < size; i++) {
    //     const char *sys_name;

    //     // Convert the system enum to a string
    //     switch (satellites[i].system) {
    //         case GNSS_SYSTEM_GPS:     sys_name = "GPS"; break;
    //         case GNSS_SYSTEM_GLONASS: sys_name = "GLONASS"; break;
    //         case GNSS_SYSTEM_GALILEO: sys_name = "Galileo"; break;
    //         case GNSS_SYSTEM_BEIDOU:  sys_name = "BeiDou"; break;
    //         case GNSS_SYSTEM_QZSS:    sys_name = "QZSS"; break;
    //         case GNSS_SYSTEM_IRNSS:   sys_name = "IRNSS"; break;
    //         case GNSS_SYSTEM_SBAS:    sys_name = "SBAS"; break;
    //         case GNSS_SYSTEM_IMES:    sys_name = "IMES"; break;
    //         default:                  sys_name = "Unknown"; break;
    //     }

    //     printf("[%2d] Sys: %-7s | PRN: %3u | SNR: %2u dB | Elev: %2u° | Azim: %3u° | Tracked: %s | Corrected: %s\n",
    //         i,
    //         sys_name,
    //         satellites[i].prn,
    //         satellites[i].snr,
    //         satellites[i].elevation,
    //         satellites[i].azimuth,
    //         satellites[i].is_tracked ? "Y" : "N",
    //         satellites[i].is_corrected ? "Y" : "N");
    //     printf("------------------------------\n");
    // }
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
            case GNSS_FIX_STATUS_ESTIMATED_FIX: fix_status_str = "Estimated Fix"; break;
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

        printf("date: %02d:%02d:%02d:%03d %02d-%02d-%04d\n", 
                data.utc.hour, data.utc.minute, data.utc.millisecond/1000, data.utc.millisecond%1000, 
                data.utc.month_day, data.utc.month, data.utc.century_year + 2000);
        printf("location: %lld %lld %d.%03d m, %d mm/s, bearing %d mdeg\n", 
                data.nav_data.latitude, data.nav_data.longitude, data.nav_data.altitude / 1000, data.nav_data.altitude % 1000, data.nav_data.speed, data.nav_data.bearing);
        printf("info: %u satellites, hdop %d.%03d, geoid separation %d.%03d m, fix status %s, fix quality %s\n", 
                data.info.satellites_cnt, data.info.hdop / 1000, data.info.hdop % 1000, data.info.geoid_separation / 1000, data.info.geoid_separation % 1000, fix_status_str, fix_quality_str);
        printf("------------------------------\n");

    }
}
    