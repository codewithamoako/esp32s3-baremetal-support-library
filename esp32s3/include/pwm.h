/*
 * PWM on the LEDC peripheral - a square wave with an adjustable duty cycle.
 *
 * Eight channels, each on almost any pin. Once started a channel runs in
 * hardware and needs no attention: no interrupt, no timer, nothing for the
 * program to keep doing. Dim an LED, drive a servo, set a motor's speed, make
 * a rough analogue voltage out of a resistor and a capacitor.
 *
 *     pwm_init(0, 6, 1000, 10);      // channel 0, GPIO6, 1 kHz, 10-bit steps
 *     pwm_set_duty(0, 512);          // half on, half off
 *
 * Frequency and resolution are a trade against each other, because both come
 * out of the same 40 MHz count: the channel divides the clock down to get its
 * period, and the leftover precision is what the duty steps are made of. The
 * product has to fit, so 40000000 / frequency is the ceiling on the number of
 * steps. At 1 kHz that allows about 15 bits, plenty; at 100 kHz it is nearer
 * 8; at 1 MHz there is almost nothing left. pwm_init() reports which way it
 * went rather than quietly producing the wrong frequency.
 *
 * The eight channels share four timers, and a timer is what actually holds a
 * frequency. Channels asking for the same frequency and resolution are put on
 * the same timer automatically, so eight LEDs dimming at 1 kHz cost one timer
 * between them. Four *different* combinations is the hard limit.
 *
 * The count comes from the 40 MHz crystal, so the frequency does not shift
 * when the CPU clock does.
 */

#ifndef PWM_H
#define PWM_H

#include <stdint.h>

// Starts a channel on a pin, at rest (duty 0) until pwm_set_duty() is called.
//
// The resolution is how many steps the duty cycle has, in bits: 8 gives 256
// steps, 10 gives 1024. More steps mean finer control and a lower ceiling on
// the frequency.
//
// - channel: 0 to 7
// - pin: the GPIO to drive
// - freq_hz: cycles per second, e.g. 1000, or 50 for a servo
// - resolution_bits: 1 to 14
// returns: 1 if the channel started, 0 if that frequency and resolution do
//          not fit together or all four timers are already spoken for
int pwm_init(uint32_t channel, uint32_t pin, uint32_t freq_hz, uint32_t resolution_bits);

// Sets how much of each cycle the pin spends high. Takes effect at the end of
// the cycle in progress, so the output never shows a runt pulse.
// - channel: 0 to 7
// - duty: 0 for always low, pwm_max_duty() for always high
// returns: nothing
void pwm_set_duty(uint32_t channel, uint32_t duty);

// The duty value that means always on, i.e. 2 to the resolution.
// - channel: 0 to 7
// returns: the top of the duty range, or 0 for a channel that was never
//          started
uint32_t pwm_max_duty(uint32_t channel);

// Stops a channel and parks its pin at a fixed level. Use this rather than a
// duty of 0 when the pin must be reliably still - a duty of 0 is a wave whose
// high phase happens to have no width, which is nearly but not quite the same
// thing. Call pwm_init() again to start the channel back up; pwm_set_duty()
// on a stopped channel changes the duty but does not restart the output.
// - channel: 0 to 7
// - idle_level: 0 to park low, 1 to park high
// returns: nothing
void pwm_stop(uint32_t channel, int idle_level);

#endif // ESP32S3_PWM_H
