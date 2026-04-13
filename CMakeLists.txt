cmake_minimum_required(VERSION 3.20.0)

set(BOARD rpi_pico2/rp2350a/m33/w)

find_package(Zephyr)
project(project)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE 
    src/main.c 
    src/battery.c
    src/altimeter.c
    src/keypad.c
    src/Ch7SegDisplay.c
)
