#ifndef CH7SEGDISPLAY_H
#define CH7SEGDISPLAY_H

#define DISPLAY_DOT_NONE  0
#define DISPLAY_DOT_RIGHT 1
#define DISPLAY_DOT_LEFT  2
#define DISPLAY_DOT_BOTH  3

bool setSegmentDisplayOn(k_timeout_t timeout);
void setSegmentDisplayOff();
void setSegmentDisplayValue(uint8_t value);
uint8_t getSegmentDisplayValue();
void setSegmentDisplayFormat(uint8_t format);
uint8_t getSegmentDisplayFormat();

#endif /* CH7SEGDISPLAY_H */