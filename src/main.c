/*
 * Bare-metal starting point for the Waveshare ESP32-S3-Zero.
 *
 * Brings the chip up and then does nothing, on purpose. That makes it two
 * useful things at once: somewhere to start writing a program, and the
 * smallest image that still proves the toolchain, the linker script and the
 * flash layout are all doing their jobs - if this one runs without rebooting,
 * anything that goes wrong next is yours rather than the build's.
 *
 * All the chip plumbing - clock, watchdogs, GPIO, console, timing, and the
 * UART, I2C, SPI and PWM buses - lives in the esp32s3.h support library, so
 * nothing about how a register or a pad works shows up here. Add the driver
 * header for whatever you need on top: led.h for the onboard RGB LED, and so
 * on. board_pins.h names the pads the way the silkscreen does.
 *
 * examples/ has a working program for each peripheral - run `make list` to
 * see them. See the README for the full story of how a bare-metal image boots
 * on this board (no ESP-IDF, no FreeRTOS, no second-stage bootloader, no
 * libc).
 */

#include "esp32s3.h"
#include "board_pins.h"


// The ROM loader jumps straight here; _start runs on the ROM's own stack.
void _start(void)
{
    // Everything the ROM left for us: zero .bss, disarm the watchdogs,
    // take the CPU from 20 MHz up to 160 MHz.
    board_init();

    // Spin forever, because there is nowhere to return to: the ROM jumped
    // here rather than calling this as a function, so falling off the end of
    // _start would run whatever bytes happen to follow it. A program without
    // an operating system underneath it has to end by never ending.
    while(1);

}
