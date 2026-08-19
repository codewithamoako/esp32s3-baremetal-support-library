/*
 * Busy-wait timing, counted with the CPU cycle counter.
 *
 * There is no timer interrupt and no scheduler here - these spin. All of them
 * scale with the CPU frequency reported by cpu_mhz(), so they stay correct
 * across a clock switch.
 */

#ifndef ESP32S3_DELAY_H
#define ESP32S3_DELAY_H

#include <stdint.h>

// The cycle counter: increments once per CPU clock, wraps after 2^32.
// returns: the current cycle count
static inline uint32_t cycle_count(void)
{
    uint32_t count;
    __asm__ __volatile__("rsr.ccount %0" : "=a"(count));
    return count;
}

// Waits the given number of CPU cycles.
// - cycles: how many cycles to wait
// returns: nothing
void delay_cycles(uint32_t cycles);

// Waits the given number of microseconds.
// - us: how many microseconds to wait
// returns: nothing
void delay_us(uint32_t us);

// Waits the given number of milliseconds.
// - ms: how many milliseconds to wait
// returns: nothing
void delay_ms(uint32_t ms);

#endif // ESP32S3_DELAY_H
