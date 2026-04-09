#ifndef BATTERY_H
#define BATTERY_H

#include <zephyr/kernel.h>

#define BAT_EVT_LOW BIT(0)
#define BAT_EVT_OK  BIT(1)

extern struct k_event batteryEvents;

#endif /* BATTERY_H */