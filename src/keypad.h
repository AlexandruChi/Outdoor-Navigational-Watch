#ifndef KEYPAD_H
#define KEYPAD_H

#define KEY_EVT_BATTERY_ON BIT(0)
#define KEY_EVT_BATTERY_OFF BIT(1)

#define KEY_EVT_NAVIGATION_POWER BIT(0)
#define KEY_EVT_NAVIGATION_LEFT BIT(1)
#define KEY_EVT_NAVIGATION_RIGHT BIT(2)
#define KEY_EVT_NAVIGATION_SELECT BIT(3)

#define KEY_EVT_COMPASS_ON BIT(0)
#define KEY_EVT_COMPASS_OFF BIT(1)

extern struct k_event batteryKeyEvent;
extern struct k_event navigationEvent;
extern struct k_event compassKeyEvent;

#endif /* KEYPAD_H */