#ifndef CONTROL_H
#define CONTROL_H

// Stick axes and fin commands both run from -CONTROL_FULL_SCALE to +CONTROL_FULL_SCALE
#define CONTROL_FULL_SCALE 1000

// Convert a raw ADC reading to a stick axis, applying the centre deadband
int control_axis_from_adc(int raw, int raw_min, int raw_center, int raw_max, int deadband);

// Mix pitch (+dive) and yaw (+right) into the two diagonal fin pairs
void control_mix_x(int pitch, int yaw, int *pair_a, int *pair_b);

// Move current towards target by at most max_step
int control_slew(int current, int target, int max_step);

#endif
