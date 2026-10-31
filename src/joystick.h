#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdbool.h>

struct joystick_state {
    int raw_x;
    int raw_y;
    int center_x;   // Resting readings from calibration
    int center_y;
    int pitch;      // +dive (stick forward), -surface
    int yaw;        // +right, -left
    bool pressed;   // Debounced push switch
};

// Set up the ADC and switch, then calibrate the centre. Leave the stick untouched.
int joystick_init(void);

// Read the stick. On error the axes are zero and the switch reads released.
int joystick_read(struct joystick_state *state);

#endif
