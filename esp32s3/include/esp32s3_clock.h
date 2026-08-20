/*
 * CPU clock.
 *
 * The ROM leaves the CPU on the crystal divided by two - 20 MHz - which is
 * too slow to bit-bang an addressable LED (see led.h). Anything that measures
 * time in CPU cycles needs to know the real frequency, so this module both
 * switches the clock and reports what it actually reads back from hardware,
 * rather than trusting a compile-time constant.
 *
 * Typical use, first thing in _start():
 *
 *     set_cpu_160mhz();
 */

#ifndef ESP32S3_CLOCK_H
#define ESP32S3_CLOCK_H

#include <stdint.h>

// Reads the current CPU speed straight from the clock-tree registers.
// returns: frequency in MHz (20, 40, 80, 160 or 240)
uint32_t read_cpu_mhz(void);

// The CPU speed that delay and LED timing use. Cached after the first call.
// returns: frequency in MHz
uint32_t cpu_mhz(void);

// Switches the CPU to 160 MHz off the PLL. Needs the PLL running, which it
// is on the ROM's boot path.
// returns: nothing
void set_cpu_160mhz(void);

// Ungates a peripheral's clock and pulses its reset, leaving it in the state
// its own driver expects to start from. Every peripheral powers up gated off
// and half-configured by the ROM, so each driver does this first.
// - clk_en_reg: SYSTEM_PERIP_CLK_EN0_REG or ..._EN1_REG
// - rst_en_reg: the matching SYSTEM_PERIP_RST_EN0_REG or ..._EN1_REG
// - mask: the peripheral's bit, the same in both registers
// returns: nothing
void periph_enable(uint32_t clk_en_reg, uint32_t rst_en_reg, uint32_t mask);

#endif // ESP32S3_CLOCK_H
