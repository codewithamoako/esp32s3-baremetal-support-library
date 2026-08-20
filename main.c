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

void _start(void)
{
    board_init();

    while(1){
    };
}
