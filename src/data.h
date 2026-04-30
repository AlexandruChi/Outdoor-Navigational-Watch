#ifndef DATA_H
#define DATA_H

#include <stdint.h>

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} time_t; // UTC

typedef struct {
    uint8_t day;
    uint8_t month;
    uint8_t year; // last two digits of the year
} date_t;

typedef struct {
    time_t time;
    date_t date;
} datetime_t;

typedef struct {
    float latitude;
    float longitude;
} position_t; // in degrees

typedef int32_t altitude_t;

typedef struct {
    float temperature; // Celsius
    float pressure; // Pa
    float humidity; // %
} environment_t;

typedef uint16_t heading_t; // degrees

typedef float battery_t; // V

typedef struct {
    char quality[10]; 
    char fixStatus[10]; 
    uint8_t satellites;
} gnss_t;

typedef struct {
    datetime_t datetime;
    position_t position;
    altitude_t altitude;
    heading_t heading;
    gnss_t gnss;
} gnss_data_t;

typedef struct {
    altitude_t altitude;
    altitude_t adjAltitude;
    environment_t environment;
} altimeter_data_t;

#endif /* DATA_H */