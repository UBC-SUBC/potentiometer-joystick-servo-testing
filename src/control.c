#include <stdlib.h>

#include "control.h"

/*
 * Convert a raw ADC reading to a stick axis.
 *
 * Each side of the centre is scaled separately because these pots rarely
 * centre at mid-scale. Output restarts from zero at the edge of the deadband
 * so there is no jump when the stick leaves it.
 */
int control_axis_from_adc(int raw, int raw_min, int raw_center, int raw_max, int deadband)
{
    int value;

    if (raw >= raw_center) {
        int span = raw_max - raw_center;

        value = (span > 0) ? (raw - raw_center) * CONTROL_FULL_SCALE / span : 0;
    } else {
        int span = raw_center - raw_min;

        value = (span > 0) ? (raw - raw_center) * CONTROL_FULL_SCALE / span : 0;
    }

    if (value > CONTROL_FULL_SCALE) {
        value = CONTROL_FULL_SCALE;
    } else if (value < -CONTROL_FULL_SCALE) {
        value = -CONTROL_FULL_SCALE;
    }

    if (abs(value) <= deadband) {
        return 0;
    }

    int outside = (value > 0) ? value - deadband : value + deadband;

    return outside * CONTROL_FULL_SCALE / (CONTROL_FULL_SCALE - deadband);
}

/*
 * X-rudder mix, matching the two-pushrod linkage this rig is modelled on.
 *
 * Each diagonal pair behaves like one rigid through-shaft, so its trailing
 * edges swing along a line at 45 degrees to the hull:
 *
 *   pair A (upper port, lower stbd): positive = up and to starboard
 *   pair B (upper stbd, lower port): positive = down and to starboard
 *
 * Dive needs every trailing edge down, right needs every one to starboard. The
 * sideways parts cancel on a dive and the vertical parts cancel on a turn.
 *
 * pitch + yaw can exceed the fin range. Both pairs are scaled down together
 * when they do, which keeps the commanded direction instead of flattening it.
 */
void control_mix_x(int pitch, int yaw, int *pair_a, int *pair_b)
{
    int a = yaw - pitch;
    int b = yaw + pitch;
    int peak = (abs(a) > abs(b)) ? abs(a) : abs(b);

    if (peak > CONTROL_FULL_SCALE) {
        a = a * CONTROL_FULL_SCALE / peak;
        b = b * CONTROL_FULL_SCALE / peak;
    }

    *pair_a = a;
    *pair_b = b;
}

int control_slew(int current, int target, int max_step)
{
    if (target > current + max_step) {
        return current + max_step;
    }
    if (target < current - max_step) {
        return current - max_step;
    }
    return target;
}
