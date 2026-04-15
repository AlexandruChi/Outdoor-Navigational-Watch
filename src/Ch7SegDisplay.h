#ifndef CH7SEGDISPLAY_H
#define CH7SEGDISPLAY_H

#include <zephyr/kernel.h>

#define DISPLAY_DOT_NONE  0b0000
#define DISPLAY_DOT_RIGHT 0b0001
#define DISPLAY_DOT_LEFT  0b0010
#define DISPLAY_DOT_BOTH  0b0011

#define DISPLAY_DIGIT_ALWAYS_NONE 0b0000
#define DISPLAY_DIGIT_ALWAYS_MIDDLE 0b0100
#define DISPLAY_DIGIT_ALWAYS_ALL 0b1000


int setSegmentDisplayOn(k_timeout_t timeout);
int setSegmentDisplayOff(k_timeout_t timeout);
int isSegmentDisplayOn(bool *val, k_timeout_t timeout);
int setSegmentDisplayValue(uint16_t value, k_timeout_t timeout);
int setSegmentDisplayFormat(uint8_t format, k_timeout_t timeout);

#endif /* CH7SEGDISPLAY_H */