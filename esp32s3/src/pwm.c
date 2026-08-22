/*
 * PWM on LEDC: four timers hold the frequencies, eight channels hold the
 * duty cycles, and channels that want the same frequency share a timer.
 */

#include "pwm.h"
#include "clock.h"
#include "gpio.h"
#include "regs.h"

// The count LEDC runs on, selected in start_peripheral(). The crystal rather
// than anything derived from the CPU clock, so a clock switch elsewhere in
// the program cannot change a servo's pulse width.
#define PWM_SRC_HZ      (ESP32S3_XTAL_MHZ * 1000000u)

// The divider is a fixed-point number with 8 fractional bits, in an 18-bit
// field: so between 1.0 and just under 1024.0.
#define PWM_DIV_MIN     (1u << 8)
#define PWM_DIV_MAX     ((1u << 18) - 1u)

#define PWM_MAX_RES_BITS 14

#define LEDC_REG(off)   ESP32S3_REG(LEDC_BASE + (off))

static struct {
    uint32_t freq_hz;
    uint32_t resolution;
    int      in_use;
} pwm_timer[LEDC_TIMERS];

static uint8_t pwm_channel_res[LEDC_CHANNELS];    // 0 until the channel starts

static void start_peripheral(void)
{
    static int done;

    if (done) {
        return;
    }
    done = 1;

    periph_enable(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_PERIP_RST_EN0_REG, SYSTEM_LEDC_CLK_EN);
    LEDC_REG(LEDC_CONF_OFF) = LEDC_GLOBAL_CLK_EN | LEDC_APB_CLK_SEL_XTAL;
}

// How far the source clock has to be divided so that one period is exactly
// 2^resolution counts long. Returns 0 if the pair does not fit the hardware,
// which is the frequency-versus-resolution trade running out.
static uint32_t divider_for(uint32_t freq_hz, uint32_t resolution)
{
    // The << 8 is the divider's fractional part, so this stays exact rather
    // than rounding the frequency to a whole number of ticks.
    uint64_t div = ((uint64_t)PWM_SRC_HZ << 8) / ((uint64_t)freq_hz << resolution);

    if (div < PWM_DIV_MIN || div > PWM_DIV_MAX) {
        return 0;
    }
    return (uint32_t)div;
}

// Finds a timer already producing this frequency and resolution, or sets a
// free one up to do it. Returns -1 when all four are busy with something else.
static int claim_timer(uint32_t freq_hz, uint32_t resolution)
{
    for (int t = 0; t < LEDC_TIMERS; t++) {
        if (pwm_timer[t].in_use
            && pwm_timer[t].freq_hz == freq_hz
            && pwm_timer[t].resolution == resolution) {
            return t;
        }
    }

    uint32_t div = divider_for(freq_hz, resolution);
    if (div == 0) {
        return -1;
    }

    for (int t = 0; t < LEDC_TIMERS; t++) {
        if (pwm_timer[t].in_use) {
            continue;
        }

        LEDC_REG(LEDC_TIMER_CONF_OFF(t)) =
            (resolution << LEDC_DUTY_RES_S) | (div << LEDC_CLK_DIV_S);
        LEDC_REG(LEDC_TIMER_CONF_OFF(t)) |= LEDC_TIMER_PARA_UP;

        // Start the counter from zero rather than from wherever it happened
        // to be, so the first cycle is a whole one.
        LEDC_REG(LEDC_TIMER_CONF_OFF(t)) |= LEDC_TIMER_RST;
        LEDC_REG(LEDC_TIMER_CONF_OFF(t)) &= ~LEDC_TIMER_RST;

        pwm_timer[t].freq_hz    = freq_hz;
        pwm_timer[t].resolution = resolution;
        pwm_timer[t].in_use     = 1;
        return t;
    }

    return -1;
}

int pwm_init(uint32_t channel, uint32_t pin, uint32_t freq_hz, uint32_t resolution_bits)
{
    if (channel >= LEDC_CHANNELS || freq_hz == 0
        || resolution_bits == 0 || resolution_bits > PWM_MAX_RES_BITS) {
        return 0;
    }

    start_peripheral();

    int timer = claim_timer(freq_hz, resolution_bits);
    if (timer < 0) {
        return 0;
    }

    // hpoint is the count at which the pin goes high; leaving it at zero puts
    // every channel's rising edge at the start of the period.
    LEDC_REG(LEDC_CH_HPOINT_OFF(channel)) = 0;
    LEDC_REG(LEDC_CH_DUTY_OFF(channel))   = 0;

    LEDC_REG(LEDC_CH_CONF0_OFF(channel)) =
        ((uint32_t)timer << LEDC_TIMER_SEL_S) | LEDC_SIG_OUT_EN;

    pwm_channel_res[channel] = (uint8_t)resolution_bits;

    // conf1 drives the fade generator. A scale of zero means no fade: the
    // duty simply becomes whatever was written and stays there.
    LEDC_REG(LEDC_CH_CONF1_OFF(channel)) =
        LEDC_DUTY_START | LEDC_DUTY_INC | (1u << LEDC_DUTY_NUM_S) | (1u << LEDC_DUTY_CYCLE_S);

    LEDC_REG(LEDC_CH_CONF0_OFF(channel)) |= LEDC_CH_PARA_UP;

    gpio_route_out(pin, LEDC_SIG(channel));
    return 1;
}

void pwm_set_duty(uint32_t channel, uint32_t duty)
{
    if (channel >= LEDC_CHANNELS || pwm_channel_res[channel] == 0) {
        return;
    }

    uint32_t max = 1u << pwm_channel_res[channel];
    if (duty > max) {
        duty = max;
    }

    // The duty register carries four fractional bits below the whole steps,
    // for a dithering mode this driver does not use.
    LEDC_REG(LEDC_CH_DUTY_OFF(channel)) = duty << LEDC_DUTY_FRAC_BITS;

    LEDC_REG(LEDC_CH_CONF1_OFF(channel)) |= LEDC_DUTY_START;
    LEDC_REG(LEDC_CH_CONF0_OFF(channel)) |= LEDC_CH_PARA_UP;
}

uint32_t pwm_max_duty(uint32_t channel)
{
    if (channel >= LEDC_CHANNELS || pwm_channel_res[channel] == 0) {
        return 0;
    }
    return 1u << pwm_channel_res[channel];
}

void pwm_stop(uint32_t channel, int idle_level)
{
    if (channel >= LEDC_CHANNELS) {
        return;
    }

    uint32_t conf0 = LEDC_REG(LEDC_CH_CONF0_OFF(channel));
    conf0 &= ~(LEDC_SIG_OUT_EN | LEDC_IDLE_LV);
    if (idle_level) {
        conf0 |= LEDC_IDLE_LV;
    }

    LEDC_REG(LEDC_CH_CONF0_OFF(channel)) = conf0;
    LEDC_REG(LEDC_CH_CONF0_OFF(channel)) = conf0 | LEDC_CH_PARA_UP;
}
