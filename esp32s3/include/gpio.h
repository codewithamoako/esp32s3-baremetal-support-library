/*
 * Pins: plain digital I/O, and the GPIO matrix that peripherals reach pads
 * through.
 *
 * Two separate things live here.
 *
 * The digital-output half is what a bit-banged LED needs: configure a pad and
 * drive it. The set/clear helpers are single stores to a write-1-to-set
 * register, so they are atomic against other pins and cheap enough to sit
 * inside a timing-critical loop.
 *
 * The matrix half is what every hardware peripheral needs. On this chip a
 * peripheral is not wired to a fixed pad: UART, I2C, SPI and LEDC each emit a
 * numbered *signal*, and a crossbar decides which pad carries it. That is why
 * uart_init() and friends take pin numbers at all - almost any pin will do.
 * gpio_route_out() and gpio_route_in() are the two sides of that crossbar, and
 * the peripheral drivers are their only expected callers.
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "regs.h"

// What a pad holds itself at when nothing is driving it.
typedef enum {
    GPIO_FLOAT = 0,
    GPIO_PULLUP,
    GPIO_PULLDOWN,
} gpio_pull_t;

// Makes a pad a digital output, ~20 mA drive, starting low.
// - pin: the GPIO number to configure
// returns: nothing
void gpio_config_output(uint32_t pin);

// Makes a pad a digital input.
// - pin: the GPIO number to configure
// - pull: what holds the pin when nothing drives it
// returns: nothing
void gpio_config_input(uint32_t pin, gpio_pull_t pull);

// Makes a pad open drain: it can pull low but never drives high, so several
// devices can share the line. This is what I2C needs on SDA and SCL.
// - pin: the GPIO number to configure
// - pull: GPIO_PULLUP to use the weak internal pull-up as the only pull-up
// returns: nothing
void gpio_config_open_drain(uint32_t pin, gpio_pull_t pull);

// Reads a pin's current level. The pad needs its input buffer on, which
// gpio_config_input() and gpio_config_open_drain() both do. Handles the pins
// above 31, which live in a second register.
// - pin: the GPIO number
// returns: 0 if low, 1 if high
int gpio_read(uint32_t pin);

// Drives a pad from a peripheral's output signal, and configures the pad to
// suit. Push-pull: UART TX, SPI clock, MOSI and CS, an LEDC channel.
// - pin: the GPIO number that should carry it
// - signal: the peripheral signal number (e.g. SPI2_CLK_SIG)
// returns: nothing
void gpio_route_out(uint32_t pin, uint32_t signal);

// Feeds a pad into a peripheral's input signal, and configures the pad as an
// input to suit. UART RX, SPI MISO.
// - pin: the GPIO number to read from
// - signal: the peripheral signal number (e.g. SPI2_MISO_SIG)
// - pull: what holds the pin when nothing drives it
// returns: nothing
void gpio_route_in(uint32_t pin, uint32_t signal, gpio_pull_t pull);

// Wires a pad to a peripheral signal in both directions at once, open drain.
// This is the shape an I2C line has: one wire that either side may pull low
// and that a pull-up returns high, with the controller watching the result.
// - pin: the GPIO number
// - signal: the peripheral signal number (e.g. I2C_SDA_SIG(0))
// - pull: GPIO_PULLUP to lean on the weak internal pull-up
// returns: nothing
void gpio_route_open_drain(uint32_t pin, uint32_t signal, gpio_pull_t pull);

// Drives a pin high. GPIO0..GPIO31 only: the pins above 31 are in a second
// register, and branching to pick one would cost cycles in the LED's bit loop.
// - pin: the GPIO number
// returns: nothing
static inline void gpio_set_high(uint32_t pin)
{
    ESP32S3_REG(GPIO_OUT_W1TS_REG) = 1u << pin;
}

// Drives a pin low. GPIO0..GPIO31 only.
// - pin: the GPIO number
// returns: nothing
static inline void gpio_set_low(uint32_t pin)
{
    ESP32S3_REG(GPIO_OUT_W1TC_REG) = 1u << pin;
}

// Drives a pin to a level. GPIO0..GPIO31 only.
// - pin: the GPIO number
// - level: 0 for low, anything else for high
// returns: nothing
static inline void gpio_write(uint32_t pin, int level)
{
    if (level) {
        gpio_set_high(pin);
    } else {
        gpio_set_low(pin);
    }
}

#endif // ESP32S3_GPIO_H
