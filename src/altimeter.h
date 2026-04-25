#ifndef ALTIMETER_H
#define ALTIMETER_H

#include <zephyr/kernel.h>
#include "data.h"

int altimeterGetData(altimeter_data_t *data, k_timeout_t timeout);

#endif /* ALTIMETER_H */