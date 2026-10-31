cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(potentiometer_joystick_servo_testing)

target_sources(app PRIVATE
    src/main.c
    src/control.c
    src/display.c
    src/fins.c
    src/joystick.c
    src/panel.c
)
