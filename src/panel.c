#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "control.h"
#include "panel.h"

#define RESET       "\x1b[0m"
#define BOLD        "\x1b[1m"
#define DIM         "\x1b[2m"
#define RED         "\x1b[91m"
#define GREEN       "\x1b[92m"
#define YELLOW      "\x1b[93m"
#define WHITE       "\x1b[97m"

// Same colours as the two pairs in the X-rudder video
#define PAIR_A_COLOR "\x1b[36m"
#define PAIR_B_COLOR "\x1b[94m"

#define RULE_10     "──────────"
#define RULE        RULE_10 RULE_10 RULE_10 RULE_10 RULE_10 "────────"

// Clear the rest of the row, then draw the right border in the last column
#define ROW_END     "\x1b[K\x1b[60G│"

#define PANEL_LAST_RULE_COLUMN 58

#define BAR_HALF    12      // Cells each side of the centre mark
#define BAR_BYTES   160

static const char *const fin_labels[FIN_COUNT] = {
    [FIN_UPPER_PORT] = "Upper port",
    [FIN_UPPER_STBD] = "Upper stbd",
    [FIN_LOWER_STBD] = "Lower stbd",
    [FIN_LOWER_PORT] = "Lower port",
};

// Matches the pair each fin is assigned to in fins.c
static const bool fin_is_pair_a[FIN_COUNT] = {
    [FIN_UPPER_PORT] = true,
    [FIN_UPPER_STBD] = false,
    [FIN_LOWER_STBD] = true,
    [FIN_LOWER_PORT] = false,
};


/*
 * Draw a command as a bar growing out from a centre mark, for example
 * ──────────┼██████────
 */
static void render_bar(char *out, int value, const char *color)
{
    int filled = (abs(value) * BAR_HALF + CONTROL_FULL_SCALE / 2) / CONTROL_FULL_SCALE;

    // Any movement off centre should be visible, however small
    if (value != 0 && filled == 0) {
        filled = 1;
    }

    int first = (value < 0) ? BAR_HALF - filled : BAR_HALF + 1;
    int last = (value < 0) ? BAR_HALF - 1 : BAR_HALF + filled;
    bool was_filled = false;

    strcpy(out, DIM);

    for (int i = 0; i <= 2 * BAR_HALF; i++) {
        bool is_filled = (i >= first && i <= last);

        if (is_filled != was_filled) {
            strcat(out, RESET);
            strcat(out, is_filled ? color : DIM);
            was_filled = is_filled;
        }

        strcat(out, (i == BAR_HALF) ? "┼" : (is_filled ? "█" : "─"));
    }

    strcat(out, RESET);
}


static void render_stick_row(char *line, const char *name, const char *negative,
                             const char *positive, int value)
{
    char bar[BAR_BYTES];

    render_bar(bar, value, WHITE);
    snprintf(line, PANEL_LINE_BYTES, "│  %-5s  %7s %s %-5s %+5d" ROW_END,
             name, negative, bar, positive, value);
}


static void render_fin_row(char *line, const struct panel_state *state, enum fin_id fin)
{
    char bar[BAR_BYTES];
    const char *color = fin_is_pair_a[fin] ? PAIR_A_COLOR : PAIR_B_COLOR;

    render_bar(bar, state->fin_command[fin], color);
    snprintf(line, PANEL_LINE_BYTES, "│  %-10s  %s%c" RESET "  %s %+5d  %4d µs" ROW_END,
             fin_labels[fin], color, fin_is_pair_a[fin] ? 'A' : 'B', bar,
             state->fin_command[fin], state->fin_pulse_us[fin]);
}


static const char *command_text(int pitch, int yaw)
{
    if (pitch > 0) {
        return (yaw > 0) ? "DIVE + RIGHT" : (yaw < 0) ? "DIVE + LEFT" : "DIVE";
    }
    if (pitch < 0) {
        return (yaw > 0) ? "SURFACE + RIGHT" : (yaw < 0) ? "SURFACE + LEFT" : "SURFACE";
    }
    return (yaw > 0) ? "RIGHT" : (yaw < 0) ? "LEFT" : "NEUTRAL";
}


void panel_render(const struct panel_state *state, char lines[PANEL_ROWS][PANEL_LINE_BYTES])
{
    char status[32];
    const char *status_color;
    int row = 0;

    if (state->test_fin != FIN_NONE) {
        snprintf(status, sizeof(status), "TEST: %s", fin_labels[state->test_fin]);
        status_color = YELLOW;
    } else if (state->armed) {
        snprintf(status, sizeof(status), "ARMED");
        status_color = RED;
    } else {
        snprintf(status, sizeof(status), "DISARMED");
        status_color = GREEN;
    }

    // Status text is " ● NAME ", ending on the last rule column before the corner
    int status_column = PANEL_LAST_RULE_COLUMN - ((int)strlen(status) + 4) + 1;

    snprintf(lines[row++], PANEL_LINE_BYTES,
             "╭" RULE "╮\x1b[3G " BOLD "X-RUDDER BENCH TEST" RESET " \x1b[%dG %s● %s" RESET " ",
             status_column, status_color, status);

    render_stick_row(lines[row++], "Pitch", "SURFACE", "DIVE", state->pitch);
    render_stick_row(lines[row++], "Yaw", "LEFT", "RIGHT", state->yaw);

    snprintf(lines[row++], PANEL_LINE_BYTES, "├" RULE "┤");

    for (int i = 0; i < FIN_COUNT; i++) {
        render_fin_row(lines[row++], state, i);
    }

    snprintf(lines[row++], PANEL_LINE_BYTES, "├" RULE "┤");

    snprintf(lines[row++], PANEL_LINE_BYTES, "│  Command  " BOLD "%s" RESET "%s" ROW_END,
             command_text(state->pitch, state->yaw),
             state->armed ? "" : DIM "  (fins held)" RESET);

    snprintf(lines[row++], PANEL_LINE_BYTES,
             "│  ADC      x %4d " DIM "(rest %4d)" RESET "   y %4d " DIM "(rest %4d)" RESET ROW_END,
             state->raw_x, state->center_x, state->raw_y, state->center_y);

    snprintf(lines[row++], PANEL_LINE_BYTES,
             "│  Event    " DIM "%02u:%02u" RESET "  %s\x1b[K\x1b[48G" DIM "up %02u:%02u:%02u" RESET
             "\x1b[60G│",
             state->event_s / 60, state->event_s % 60, state->event,
             state->uptime_s / 3600, (state->uptime_s / 60) % 60, state->uptime_s % 60);

    snprintf(lines[row++], PANEL_LINE_BYTES, "╰" RULE "╯\x1b[3G " DIM "%s" RESET " ",
             state->armed ? "press stick to disarm"
                          : "press stick to arm · buttons 1-4 test one fin");
}
