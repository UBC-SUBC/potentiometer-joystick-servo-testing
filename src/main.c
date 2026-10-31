#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <dk_buttons_and_leds.h>

#include "control.h"
#include "display.h"
#include "fins.h"
#include "joystick.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define LOOP_PERIOD_MS      20
#define IDENTIFY_MS         1000
#define ARMED_LED           DK_LED1

static atomic_t identify_request = ATOMIC_INIT(FIN_NONE);
static struct panel_state panel = { .test_fin = FIN_NONE };


/*
 * Buttons 1-4 pick one fin to deflect. The move itself happens in the main
 * loop, since this handler runs on the system workqueue and must not block.
 */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    static const uint32_t masks[FIN_COUNT] = {
        [FIN_UPPER_PORT] = DK_BTN1_MSK,
        [FIN_UPPER_STBD] = DK_BTN2_MSK,
        [FIN_LOWER_STBD] = DK_BTN3_MSK,
        [FIN_LOWER_PORT] = DK_BTN4_MSK,
    };
    uint32_t pressed = button_state & has_changed;

    for (int i = 0; i < FIN_COUNT; i++) {
        if (pressed & masks[i]) {
            atomic_set(&identify_request, i);
        }
    }
}


static void report_event(const char *event)
{
    panel.event = event;
    panel.event_s = k_uptime_get() / MSEC_PER_SEC;
}


static void set_armed(bool armed)
{
    panel.armed = armed;
    dk_set_led(ARMED_LED, armed);
    report_event(armed ? "Armed" : "Disarmed");
}


int main(void)
{
    bool was_pressed = false;
    bool joystick_ok = true;
    bool fins_ok = true;
    int64_t identify_until = 0;

    LOG_INF("X-rudder joystick servo test starting");

    if (dk_leds_init()) {
        LOG_ERR("Failed to initialize the LEDs library");
    }

    if (dk_buttons_init(button_handler)) {
        LOG_ERR("Failed to initialize the buttons library");
    }

    // Servos first, so they are holding neutral even if the joystick check fails
    if (fins_init() != 0) {
        LOG_ERR("Servo initialization failed");
        return 0;
    }

    if (joystick_init() != 0) {
        LOG_ERR("Joystick initialization failed. Fix it and reset, fins stay at neutral");
        return 0;
    }

    report_event("Ready");
    display_start();

    while (true) {
        struct joystick_state joy;
        int err = joystick_read(&joy);
        int64_t now = k_uptime_get();

        if ((err == 0) != joystick_ok) {
            joystick_ok = (err == 0);
            report_event(joystick_ok ? "Joystick read recovered" : "Joystick read failed");
        }

        if (!joystick_ok && panel.armed) {
            set_armed(false);
        }

        if (joy.pressed && !was_pressed) {
            panel.test_fin = FIN_NONE;
            set_armed(!panel.armed);
        }
        was_pressed = joy.pressed;

        int request = atomic_set(&identify_request, FIN_NONE);

        if (request != FIN_NONE) {
            if (panel.armed) {
                report_event("Disarm to test one fin");
            } else {
                panel.test_fin = request;
                identify_until = now + IDENTIFY_MS;
                report_event("Testing one fin for 1 s");
            }
        }

        if (panel.armed) {
            int pair_a;
            int pair_b;

            control_mix_x(joy.pitch, joy.yaw, &pair_a, &pair_b);
            fins_set_pair_targets(pair_a, pair_b);
        } else if (panel.test_fin != FIN_NONE && now < identify_until) {
            fins_set_single_target(panel.test_fin, CONTROL_FULL_SCALE);
        } else {
            panel.test_fin = FIN_NONE;
            fins_set_pair_targets(0, 0);
        }

        // On failure the next loop tries again
        err = fins_update();

        if ((err == 0) != fins_ok) {
            fins_ok = (err == 0);
            report_event(fins_ok ? "Servo output recovered" : "Servo output error");
        }

        panel.pitch = joy.pitch;
        panel.yaw = joy.yaw;
        panel.raw_x = joy.raw_x;
        panel.raw_y = joy.raw_y;
        panel.center_x = joy.center_x;
        panel.center_y = joy.center_y;
        panel.uptime_s = now / MSEC_PER_SEC;

        for (int i = 0; i < FIN_COUNT; i++) {
            panel.fin_command[i] = fins_command(i);
            panel.fin_pulse_us[i] = fins_pulse_us(i);
        }

        display_update(&panel);

        k_msleep(LOOP_PERIOD_MS);
    }

    return 0;
}
