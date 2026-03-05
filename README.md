# Outdoor Navigational Watch

The project aims at building a watch designed to offer real-time information about the current UTC time, geographical position, and directional orientation, as well as environmental data including atmospheric pressure, calculated altitude, and ambient temperature.

## Hardware

### Processing Unit
* **Microcontroller:** Raspberry Pi Pico 2 (RP2350, utilising dual ARM Cortex-M33 cores)

### Sensors
* **GNSS Module:** GY-NEO6MV2 (Geographical Position & UTC Time)
* **Magnetometer:** HW-246 / GY-271 (Directional Compass)
* **Atmospheric Sensor:** GY-68 / BMP180 (Atmospheric Pressure & Ambient Temperature)

### Display
* **Main Screen:** GME12864 (128x64 Monochrome OLED)
* **Battery Readout:** 7-Segment Display
* **Status Indicator:** Coloured LED

### User Input
* **Interface Buttons:** Buttons for menu navigation and selection
* **Battery Button:** Button to show the battery voltage
* **Display Toggle:** Button to disable/enable the main screen

## Software

### Firmware
* **Operating System:** Zephyr RTOS
* **Graphics Library:** LVGL
* **Language:** C

### Input
* **GNSS Module:** Reading data over UART
* **User Input:** GPIO interrupts from the buttons
* **Magnetometer:** Hardware interrupt when new data is available
* **Atmospheric Sensor:** Sending read commands to the sensor over I2C
* **Battery Level:** Analog-to-Digital Converter monitoring:
    * **On-Demand:** Hardware interrupt triggered by the battery status button
    * **Scheduled:** Periodic polling to monitor voltage for low-battery warning

### Output
* **Main Screen:** Managed via Zephyr display libraries
* **Battery Readout:** Driven via GPIO for the 7-segment display
* **Status Indicator:** Driven via GPIO PWM for LED control
