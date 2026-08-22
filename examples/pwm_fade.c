/*
 * PWM: an LED breathing on one channel, a servo sweeping on another.
 *
 *   make pwm_fade flash monitor
 *
 * Wiring:
 *
 *   GPIO6 ---- 330R ---- LED ---- GND        (the fading channel)
 *   GPIO7 ---- servo signal wire             (the sweeping channel)
 *
 * The board's own RGB LED cannot be used here: it is a WS2812, which wants a
 * serial data stream rather than a duty cycle. Any ordinary LED will do.
 *
 * The two channels show the trade the LEDC peripheral makes. Both are driven
 * from the same 40 MHz count, and each divides it down to get its period; the
 * precision left over afterwards is what the duty steps are made of. So the
 * LED at 1 kHz can afford 12 bits of duty, while the servo at 50 Hz gets 14
 * bits despite being twenty times slower - the ceiling is the *product*, and
 * 14 bits is simply where the hardware stops.
 *
 * Because they ask for different frequencies the two channels land on
 * different timers. There are four, so four distinct frequencies at once.
 *
 * Once set, a channel runs entirely in hardware. The delay_ms() calls below
 * are pacing the fade, not maintaining the waveform - the pin would keep
 * pulsing correctly if this loop stopped.
 */

#include "board.h"
#include "board_pins.h"

#define LED_CHANNEL     0
#define LED_PIN         PIN_GPIO6
#define LED_HZ          1000
#define LED_BITS        12          // 4096 duty steps

#define SERVO_CHANNEL   1
#define SERVO_PIN       PIN_GPIO7
#define SERVO_HZ        50          // one pulse every 20 ms
#define SERVO_BITS      14          // 16384 steps across those 20 ms

// A hobby servo reads the *width* of the high pulse, not the duty cycle:
// about 1 ms means one end of its travel, 2 ms the other. Converting a width
// in microseconds to a duty value is what this does - at 50 Hz one period is
// 20000 us, so the duty is that fraction of the full scale.
static uint32_t servo_duty(uint32_t microseconds)
{
    return (pwm_max_duty(SERVO_CHANNEL) * microseconds) / 20000u;
}

void _start(void)
{
    board_init();

    if (!pwm_init(LED_CHANNEL, LED_PIN, LED_HZ, LED_BITS)) {
        console_print("led channel refused - frequency and resolution do not fit\r\n");
    }
    if (!pwm_init(SERVO_CHANNEL, SERVO_PIN, SERVO_HZ, SERVO_BITS)) {
        console_print("servo channel refused\r\n");
    }

    console_print("led on gpio");
    console_print_u32(LED_PIN);
    console_print(" at ");
    console_print_u32(LED_HZ);
    console_print(" Hz, duty 0..");
    console_print_u32(pwm_max_duty(LED_CHANNEL));
    console_print("\r\n");

    console_print("servo on gpio");
    console_print_u32(SERVO_PIN);
    console_print(" at ");
    console_print_u32(SERVO_HZ);
    console_print(" Hz, 1ms=");
    console_print_u32(servo_duty(1000));
    console_print(" 2ms=");
    console_print_u32(servo_duty(2000));
    console_print("\r\n");

    uint32_t top = pwm_max_duty(LED_CHANNEL);

    for (;;) {
        // The eye's response to brightness is closer to logarithmic than
        // linear, so a duty that climbs in equal steps looks like it rushes
        // the dim end and crawls at the bright one. Squaring the ramp gets
        // much nearer to an even-looking fade.
        for (uint32_t step = 0; step <= 64; step++) {
            pwm_set_duty(LED_CHANNEL, (step * step * top) / (64 * 64));
            delay_ms(15);
        }
        for (uint32_t step = 64; step > 0; step--) {
            pwm_set_duty(LED_CHANNEL, (step * step * top) / (64 * 64));
            delay_ms(15);
        }

        // One sweep of the servo per breath of the LED.
        pwm_set_duty(SERVO_CHANNEL, servo_duty(1000));
        delay_ms(500);
        pwm_set_duty(SERVO_CHANNEL, servo_duty(1500));
        delay_ms(500);
        pwm_set_duty(SERVO_CHANNEL, servo_duty(2000));
        delay_ms(500);
    }
}
