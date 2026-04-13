#ifndef CH7SEGDISPLAY_H
#define CH7SEGDISPLAY_H

#define DISPLAY_DOT_NONE  0
#define DISPLAY_DOT_RIGHT 1
#define DISPLAY_DOT_LEFT  2
#define DISPLAY_DOT_BOTH  3

void setSegmentDisplayOn();
void setSegmentDisplayOff();
void setSegmentDisplayValue(uint16_t value);
void setSegmentDisplayFormat(uint8_t format);

#endif /* CH7SEGDISPLAY_H */