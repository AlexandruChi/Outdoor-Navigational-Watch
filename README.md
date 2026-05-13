# Outdoor Navigational Watch

The project aims at building a watch designed to offer real-time information about the current UTC time, geographical position, and directional orientation, as well as environmental data including atmospheric pressure, calculated altitude, ambient temperature, ambient humidity.

## Hardware

### Processing Unit
* **Microcontroller:** Raspberry Pi Pico 2W (RP2350, utilising dual ARM Cortex-M33 cores)

### Sensors
* **GNSS Module:** ATGM336H-5N (Geographical Position & UTC Time)
* **Magnetometer:** QMC5883L (Directional Compass)
* **Atmospheric Sensor:** BME280 (Atmospheric Pressure, Ambient Temperature, Ambient Humidity)

### Display
* **Main Screen:** GME12864 (128x64 Monochrome OLED)
* **Compass & Battery:** 7-Segment Display
* **Status Indicator:** Coloured LED

### User Input

4 buttons:

* **Power button**
    * **Press:** toggle the display on and off
    * **Hold:** show battery voltage
* **Navigation left**
    * **Press:** previous display page
    * **Hold:** start / cancel compass calibration
* **Navigation right**
    * **Press:** next display page
    * **Hold** for 5 seconds to reset compass calibration
* **Action button**
    * **Press:** change displayed unit
    * **Hold:** toggle compass reading

## Software

### Firmware
* **Operating System:** Zephyr RTOS
* **Graphics Library:** Zephyr character framebuffer
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
* **Compass & Battery:** Driven via GPIO for the 7-segment display
* **Status Indicator:** Driven via GPIO for LED control
