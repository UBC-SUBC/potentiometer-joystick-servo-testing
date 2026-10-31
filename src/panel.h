#ifndef PANEL_H
#define PANEL_H

#include <stdbool.h>

#include "fins.h"

#define PANEL_ROWS          13
#define PANEL_LINE_BYTES    320

// Everything shown on the serial status panel
struct panel_state {
    bool armed;
    int test_fin;           // Fin being tested on its own, or FIN_NONE
    int pitch;
    int yaw;
    int raw_x;
    int raw_y;
    int center_x;
    int center_y;
    int fin_command[FIN_COUNT];
    int fin_pulse_us[FIN_COUNT];
    const char *event;      // Most recent event, must stay valid
    unsigned int event_s;   // Uptime when it happened
    unsigned int uptime_s;
};

// Format the panel into one string per row, including colours and cursor moves
void panel_render(const struct panel_state *state, char lines[PANEL_ROWS][PANEL_LINE_BYTES]);

#endif
