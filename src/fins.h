#ifndef FINS_H
#define FINS_H

// Fins as seen from the stern, looking forward, clockwise from upper left
enum fin_id {
    FIN_UPPER_PORT,
    FIN_UPPER_STBD,
    FIN_LOWER_STBD,
    FIN_LOWER_PORT,
    FIN_COUNT,
};

#define FIN_NONE -1

// Check the PWM device and bring each servo to neutral, one at a time
int fins_init(void);

// Set targets for the two diagonal pairs, from control_mix_x()
void fins_set_pair_targets(int pair_a, int pair_b);

// Set one fin's target and return the other three to neutral
void fins_set_single_target(enum fin_id fin, int command);

// Step every fin towards its target and write the PWM outputs. Call every loop.
int fins_update(void);

// Current command of a fin, after the speed limit and before reversing
int fins_command(enum fin_id fin);

// Current pulse width of a fin in microseconds
int fins_pulse_us(enum fin_id fin);

#endif
