#ifndef ALTIMETER_H
#define ALTIMETER_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#define UPDATE_INTERVAL_MS 1000

int getAltimeterData(struct sensor_value *pressure, struct sensor_value *dieTemp, struct sensor_value *altitude, struct sensor_value *adjAltitude, k_timeout_t timeout);

#endif /* ALTIMETER_H */