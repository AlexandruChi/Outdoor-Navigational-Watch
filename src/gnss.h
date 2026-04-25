#ifndef GNSS_H
#define GNSS_H

#include <zephyr/kernel.h>
#include "data.h"

int gnssGetData(gnss_data_t *data, k_timeout_t timeout);

#endif /* GNSS_H */