#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#include <errno.h>
#include <stdbool.h>

#include "control.h"
#include "fins.h"

LOG_MODULE_REGISTER(fins, LOG_LEVEL_INF);

#define SERVO_CENTER_US     1500

/*
 * Pulse change at full fin command. The SER0062 covers 180 degrees across
 * 500-2500 us (11.1 us per degree), so 333 us is about +/-30 degrees at the
 * servo. Check for binding before raising this with linkages attached.
 */
#define FIN_TRAVEL_US       333

/*
 * Largest command change per 20 ms loop: 60 is about 90 degrees per second.
 * Well under the servo's top speed, which limits the current drawn by four
 * high-torque servos all starting at once.
 */
#define FIN_SLEW_PER_LOOP   60

#define FIN_STARTUP_GAP_MS  300

enum fin_pair {
    PAIR_A,     // Upper port + lower stbd, positive = trailing edge up and to starboard
    PAIR_B,     // Upper stbd + lower port, positive = trailing edge down and to starboard
};

struct fin {
    const char *name;
    struct pwm_dt_spec pwm;
    uint32_t min_pulse_ns;
    uint32_t max_pulse_ns;
    enum fin_pair pair;
    bool reversed;
    int trim_us;
};

#define FIN(node_label, fin_pair, is_reversed, trim)                       \
    {                                                                      \
        .name = #node_label,                                               \
        .pwm = PWM_DT_SPEC_GET(DT_NODELABEL(node_label)),                  \
        .min_pulse_ns = DT_PROP(DT_NODELABEL(node_label), min_pulse),      \
        .max_pulse_ns = DT_PROP(DT_NODELABEL(node_label), max_pulse),      \
        .pair = fin_pair,                                                  \
        .reversed = is_reversed,                                           \
        .trim_us = trim,                                                   \
    }

/*
 * Per-fin setup: reversed and trim_us are the values to tune on the bench.
 *
 * The lower fins start reversed because four servos mounted identically with
 * their shafts pointing outward all turn the same way around the hull, while
 * each pair has to act as one through-shaft, so opposite fins must turn opposite
 * ways as seen from outboard. If everything is backwards, flip all four.
 */
static const struct fin fins[FIN_COUNT] = {
    [FIN_UPPER_PORT] = FIN(fin_upper_port, PAIR_A, false, 0),
    [FIN_UPPER_STBD] = FIN(fin_upper_stbd, PAIR_B, false, 0),
    [FIN_LOWER_STBD] = FIN(fin_lower_stbd, PAIR_A, true, 0),
    [FIN_LOWER_PORT] = FIN(fin_lower_port, PAIR_B, true, 0),
};

static int commands[FIN_COUNT];
static int targets[FIN_COUNT];
static uint32_t written_ns[FIN_COUNT];


static uint32_t pulse_ns_for(const struct fin *fin, int command)
{
    int offset_us = command * FIN_TRAVEL_US / CONTROL_FULL_SCALE;

    if (fin->reversed) {
        offset_us = -offset_us;
    }

    uint32_t pulse_ns = PWM_USEC(SERVO_CENTER_US + fin->trim_us + offset_us);

    return CLAMP(pulse_ns, fin->min_pulse_ns, fin->max_pulse_ns);
}


static int write_fin(enum fin_id id, int command)
{
    uint32_t pulse_ns = pulse_ns_for(&fins[id], command);

    if (pulse_ns == written_ns[id]) {
        return 0;
    }

    int err = pwm_set_pulse_dt(&fins[id].pwm, pulse_ns);

    if (err != 0) {
        LOG_ERR("Failed to set %s: %d", fins[id].name, err);
        return err;
    }

    written_ns[id] = pulse_ns;

    return 0;
}


int fins_init(void)
{
    for (int i = 0; i < FIN_COUNT; i++) {
        if (!pwm_is_ready_dt(&fins[i].pwm)) {
            LOG_ERR("PWM device %s is not ready", fins[i].pwm.dev->name);
            return -ENODEV;
        }
    }

    /*
     * A servo jumps straight to the first position it is sent. Staggering them
     * stops all four from pulling peak current together at power up.
     */
    for (int i = 0; i < FIN_COUNT; i++) {
        int err = write_fin(i, 0);

        if (err != 0) {
            return err;
        }

        k_msleep(FIN_STARTUP_GAP_MS);
    }

    return 0;
}


void fins_set_pair_targets(int pair_a, int pair_b)
{
    for (int i = 0; i < FIN_COUNT; i++) {
        targets[i] = (fins[i].pair == PAIR_A) ? pair_a : pair_b;
    }
}


void fins_set_single_target(enum fin_id fin, int command)
{
    for (int i = 0; i < FIN_COUNT; i++) {
        targets[i] = (i == fin) ? command : 0;
    }
}


int fins_update(void)
{
    int result = 0;

    for (int i = 0; i < FIN_COUNT; i++) {
        commands[i] = control_slew(commands[i], targets[i], FIN_SLEW_PER_LOOP);

        int err = write_fin(i, commands[i]);

        if (err != 0) {
            result = err;
        }
    }

    return result;
}


int fins_command(enum fin_id fin)
{
    return commands[fin];
}


int fins_pulse_us(enum fin_id fin)
{
    return written_ns[fin] / NSEC_PER_USEC;
}
