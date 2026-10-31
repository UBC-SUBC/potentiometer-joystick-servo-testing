#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <string.h>

#include "display.h"

#define DISPLAY_STACK_SIZE      2048
#define DISPLAY_PRIORITY        K_PRIO_PREEMPT(10)
#define DISPLAY_PERIOD_MS       100

// Redraw everything now and then so a terminal opened after boot still gets the panel
#define DISPLAY_FULL_EVERY      20

// Gives boot messages time to print before the screen is cleared
#define DISPLAY_START_DELAY_MS  100

#define HIDE_CURSOR             "\x1b[?25l"
#define CLEAR_SCREEN            "\x1b[2J"

K_THREAD_STACK_DEFINE(display_stack, DISPLAY_STACK_SIZE);
K_MUTEX_DEFINE(display_lock);

static struct k_thread display_thread_data;
static struct panel_state latest;
static char rendered[PANEL_ROWS][PANEL_LINE_BYTES];
static char shown[PANEL_ROWS][PANEL_LINE_BYTES];


/*
 * printk busy-waits on the UART, which at 115200 baud is far slower than the
 * 20 ms control loop. Drawing in a low priority thread lets that loop preempt it,
 * and only sending rows that changed keeps the traffic small.
 */
static void display_thread(void *arg1, void *arg2, void *arg3)
{
    uint32_t frames = 0;

    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    k_msleep(DISPLAY_START_DELAY_MS);
    printk(HIDE_CURSOR CLEAR_SCREEN);

    while (true) {
        struct panel_state snapshot;

        k_mutex_lock(&display_lock, K_FOREVER);
        snapshot = latest;
        k_mutex_unlock(&display_lock);

        panel_render(&snapshot, rendered);

        bool draw_all = (frames++ % DISPLAY_FULL_EVERY) == 0;
        bool drew = false;

        for (int row = 0; row < PANEL_ROWS; row++) {
            if (draw_all || strcmp(rendered[row], shown[row]) != 0) {
                printk("\x1b[%d;1H%s", row + 1, rendered[row]);
                strcpy(shown[row], rendered[row]);
                drew = true;
            }
        }

        // Leave the cursor under the panel so any error log prints below it
        if (drew) {
            printk("\x1b[%d;1H", PANEL_ROWS + 1);
        }

        k_msleep(DISPLAY_PERIOD_MS);
    }
}


void display_start(void)
{
    k_thread_create(&display_thread_data, display_stack,
                    K_THREAD_STACK_SIZEOF(display_stack),
                    display_thread, NULL, NULL, NULL,
                    DISPLAY_PRIORITY, 0, K_NO_WAIT);
}


void display_update(const struct panel_state *state)
{
    k_mutex_lock(&display_lock, K_FOREVER);
    latest = *state;
    k_mutex_unlock(&display_lock);
}
