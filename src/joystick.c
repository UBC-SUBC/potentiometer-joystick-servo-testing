#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#include <errno.h>

#include "control.h"
#include "joystick.h"

LOG_MODULE_REGISTER(joystick, LOG_LEVEL_INF);

// Flip these if a stick direction comes out backwards for how your module is mounted
#define JOY_INVERT_PITCH    false
#define JOY_INVERT_YAW      false

#define JOY_RAW_MAX         4095    // 12-bit ADC, see the overlay
#define JOY_RAIL_MARGIN     100     // Treat readings this close to the rails as full stick
#define JOY_DEADBAND        80      // 8% of full scale around the centre

#define JOY_CAL_SAMPLES     32
#define JOY_CENTER_MIN      (JOY_RAW_MAX / 4)
#define JOY_CENTER_MAX      (JOY_RAW_MAX * 3 / 4)

#define ZEPHYR_USER DT_PATH(zephyr_user)

static const struct adc_dt_spec axis_x = ADC_DT_SPEC_GET_BY_IDX(ZEPHYR_USER, 0);
static const struct adc_dt_spec axis_y = ADC_DT_SPEC_GET_BY_IDX(ZEPHYR_USER, 1);
static const struct gpio_dt_spec sw = GPIO_DT_SPEC_GET(ZEPHYR_USER, joystick_sw_gpios);

static int center_x;
static int center_y;
static bool sw_previous;
static bool sw_debounced;


static int read_raw(const struct adc_dt_spec *axis, int *raw)
{
    int16_t sample;
    struct adc_sequence sequence = {
        .buffer = &sample,
        .buffer_size = sizeof(sample),
    };

    int err = adc_sequence_init_dt(axis, &sequence);

    if (err == 0) {
        err = adc_read_dt(axis, &sequence);
    }
    if (err != 0) {
        return err;
    }

    *raw = sample;

    return 0;
}


static int setup_axis(const struct adc_dt_spec *axis)
{
    if (!adc_is_ready_dt(axis)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }

    return adc_channel_setup_dt(axis);
}


/*
 * Average the resting position of one axis and sanity check it. A centre far
 * from mid-scale means a stick being held, a floating wire, or the module
 * powered from 5 V instead of VDD.
 */
static int calibrate_axis(const struct adc_dt_spec *axis, const char *name, int *center)
{
    int sum = 0;

    for (int i = 0; i < JOY_CAL_SAMPLES; i++) {
        int raw;
        int err = read_raw(axis, &raw);

        if (err != 0) {
            return err;
        }

        sum += raw;
        k_msleep(5);
    }

    *center = sum / JOY_CAL_SAMPLES;

    LOG_INF("%s centre: %d", name, *center);

    if (*center < JOY_CENTER_MIN || *center > JOY_CENTER_MAX) {
        LOG_ERR("%s centre outside %d-%d. Check wiring, supply is VDD not 5 V, "
                "and stick is released", name, JOY_CENTER_MIN, JOY_CENTER_MAX);
        return -EIO;
    }

    return 0;
}


int joystick_init(void)
{
    int err = setup_axis(&axis_x);

    if (err == 0) {
        err = setup_axis(&axis_y);
    }
    if (err != 0) {
        return err;
    }

    if (!gpio_is_ready_dt(&sw)) {
        LOG_ERR("Joystick switch GPIO not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&sw, GPIO_INPUT);
    if (err != 0) {
        return err;
    }

    err = calibrate_axis(&axis_x, "VRx", &center_x);
    if (err != 0) {
        return err;
    }

    return calibrate_axis(&axis_y, "VRy", &center_y);
}


int joystick_read(struct joystick_state *state)
{
    *state = (struct joystick_state){
        .center_x = center_x,
        .center_y = center_y,
    };

    int raw_x;
    int raw_y;
    int err = read_raw(&axis_x, &raw_x);

    if (err == 0) {
        err = read_raw(&axis_y, &raw_y);
    }
    if (err != 0) {
        return err;
    }

    int sw_now = gpio_pin_get_dt(&sw);

    if (sw_now < 0) {
        return sw_now;
    }

    // Polling every 20 ms, so two matching reads are enough to reject bounce
    if ((sw_now != 0) == sw_previous) {
        sw_debounced = sw_previous;
    }
    sw_previous = (sw_now != 0);

    int yaw = control_axis_from_adc(raw_x, JOY_RAIL_MARGIN, center_x,
                                    JOY_RAW_MAX - JOY_RAIL_MARGIN, JOY_DEADBAND);
    int pitch = control_axis_from_adc(raw_y, JOY_RAIL_MARGIN, center_y,
                                      JOY_RAW_MAX - JOY_RAIL_MARGIN, JOY_DEADBAND);

    state->raw_x = raw_x;
    state->raw_y = raw_y;
    state->yaw = JOY_INVERT_YAW ? -yaw : yaw;
    state->pitch = JOY_INVERT_PITCH ? -pitch : pitch;
    state->pressed = sw_debounced;

    return 0;
}
