/*
 * Addressable RGB LED (WS2812 / WS2812B) driver, bit-banged on one GPIO.
 *
 * An addressable LED has no clock line: each bit is a high pulse whose
 * *length* says whether it is a zero or a one, and the whole frame has to go
 * out without a gap. This driver times both edges from the CPU cycle counter
 * and masks interrupts for the duration of a frame, which is about 30 us per
 * pixel.
 *
 * Colours go in as plain RGB; the GRB wire order the part actually wants is
 * this driver's problem, not the caller's.
 *
 *     led_init(21);
 *     led_set_color(LED_RGB(32, 5, 1));
 *
 * One strip at a time. led_init() picks which pin that is.
 *
 * Requires a CPU fast enough to resolve a 350 ns pulse from an 800 ns one -
 * call set_cpu_160mhz() first. At the 20 MHz the ROM leaves behind, every bit
 * reads as a one and the LED simply shows the wrong colour.
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_color_t;

// Usable anywhere a led_color_t is: as an argument, or as an initialiser.
#define LED_RGB(r, g, b) ((led_color_t){ (uint8_t)(r), (uint8_t)(g), (uint8_t)(b) })

#define LED_OFF      LED_RGB(0, 0, 0)
#define LED_RED      LED_RGB(10, 0, 0)
#define LED_GREEN    LED_RGB(0, 10, 0)
#define LED_BLUE     LED_RGB(0, 0, 10)
#define LED_WHITE    LED_RGB(10, 10, 10)

// Configures the data pin and drives it low. Call once before writing.
// - data_pin: GPIO number wired to the LED data line
// returns: nothing
void led_init(uint32_t data_pin);

// Sends one frame: pixel 0 is the LED nearest the controller.
// - pixels: pointer to an array of colors, one per LED
// - pixel_count: number of pixels in the array
// returns: nothing
void led_write(const led_color_t *pixels, uint32_t pixel_count);

// Shows a single color on a one-LED board.
// - color: the color to display
// returns: nothing
void led_set_color(led_color_t color);

#endif // LED_H
