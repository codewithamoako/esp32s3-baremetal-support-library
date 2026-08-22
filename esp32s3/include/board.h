/*
 * Bare-metal support library for the ESP32-S3 - the umbrella header.
 *
 * Include this one file to get the whole chip-support layer: clock, timing,
 * GPIO, console, watchdogs, and the four hardware buses - UART, I2C, SPI and
 * PWM. Drivers for things you plug *into* those buses, such as led.h, are
 * separate and included on their own.
 *
 * Everything here is polled: no interrupt handler, no buffering behind your
 * back, no scheduler. A call returns when the hardware has finished.
 *
 * The buses are all clocked from the 40 MHz crystal or the 80 MHz PLL rather
 * than from anything the CPU clock feeds, so a baud rate or a servo pulse
 * cannot drift because set_cpu_160mhz() ran.
 *
 * No ESP-IDF, no FreeRTOS, no second-stage bootloader, no libc. An image
 * built against this is written straight to flash offset 0, and the ESP32-S3
 * ROM loader copies its segments into SRAM and jumps to _start.
 *
 * A program looks like this:
 *
 *     #include "board.h"
 *
 *     void _start(void)
 *     {
 *         board_init();
 *         console_print("hello\r\n");
 *         for (;;) { delay_ms(1000); }
 *     }
 *
 * _start() can be a plain C function because we inherit the ROM's exception
 * vectors (VECBASE is left alone), which is what keeps Xtensa register-window
 * overflow and underflow working without a handler of our own. We also
 * inherit the ROM's stack, exactly as ESP-IDF's own bootloader does.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#include "clock.h"
#include "console.h"
#include "delay.h"
#include "gpio.h"
#include "i2c.h"
#include "pwm.h"
#include "regs.h"
#include "spi.h"
#include "uart.h"
#include "watchdog.h"
#include "adc.h"

// Boots the chip: zeroes .bss, disables the watchdogs, and raises the CPU
// to 160 MHz. Call first in _start(), before anything reads a global.
// returns: nothing
void board_init(void);

// The CPU speed the ROM handed over, before board_init() sped it up.
// returns: frequency in MHz (e.g. 20)
uint32_t boot_cpu_mhz(void);

#endif // ESP32S3_H
