#ifndef DISPLAY_H
#define DISPLAY_H

#include "panel.h"

// Clear the terminal and start drawing the status panel
void display_start(void);

// Give the panel new values. Safe to call from the control loop, it never waits on the UART.
void display_update(const struct panel_state *state);

#endif
