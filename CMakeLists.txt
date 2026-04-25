cmake_minimum_required(VERSION 3.20.0)

set(OPENOCD "/usr/local/bin/openocd")
set(BOARD rpi_pico2/rp2350a/m33/w)

find_package(Zephyr)
project(project)

target_sources(app PRIVATE 
    src/main.c 
    src/battery.c
    src/altimeter.c
    src/compass.c
    src/keypad.c
    src/gnss.c
    src/display.c
    src/Ch7SegDisplay.c
)
