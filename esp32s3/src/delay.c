/*
 * Busy-wait timing, counted with the CPU cycle counter.
 */

#include "delay.h"
#include "clock.h"

void delay_cycles(uint32_t cycles)
{
    uint32_t start = cycle_count();
    while (cycle_count() - start < cycles) { }
}

void delay_us(uint32_t us)
{
    delay_cycles(us * cpu_mhz());
}

// Stepping 1 ms at a time keeps the cycle count well clear of overflow:
// at 240 MHz a single delay_cycles() call can only span ~17 s.
void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}
