/*
 * Addressable RGB LED (WS2812 / WS2812B) driver, bit-banged on one GPIO.
 */

#include "led.h"
#include "clock.h"
#include "delay.h"
#include "gpio.h"

// Wire timing, in nanoseconds, from the WS2812B datasheet. Each bit is a high
// phase followed by a low phase; only the split between them carries data.
#define LED_T0H_NS       350
#define LED_T0_BIT_NS    (350 + 900)
#define LED_T1H_NS       800
#define LED_T1_BIT_NS    (800 + 450)
#define LED_RESET_US     300     // WS2812B-V5 wants >280 us of idle low

// Nanoseconds -> CPU cycles at the current clock, rounded up.
#define LED_CYCLES(ns)   (((ns) * cpu_mhz() + 999u) / 1000u)

// Cycle counts for one bit, worked out once per frame.
typedef struct {
    uint32_t zero_high;
    uint32_t zero_period;
    uint32_t one_high;
    uint32_t one_period;
} led_timing_t;

static uint32_t led_pin;

void led_init(uint32_t data_pin)
{
    led_pin = data_pin;
    gpio_config_output(data_pin);
}

// One byte, most significant bit first.
static void led_send_byte(uint8_t byte, const led_timing_t *timing)
{
    for (int bit = 0; bit < 8; bit++) {
        uint32_t high   = (byte & 0x80) ? timing->one_high   : timing->zero_high;
        uint32_t period = (byte & 0x80) ? timing->one_period : timing->zero_period;
        byte <<= 1;

        // Both edges are timed from one base, so the bit period cannot drift
        // even if the high phase overshoots slightly.
        uint32_t start = cycle_count();
        gpio_set_high(led_pin);
        while (cycle_count() - start < high)   { }
        gpio_set_low(led_pin);
        while (cycle_count() - start < period) { }
    }
}

void led_write(const led_color_t *pixels, uint32_t pixel_count)
{
    const led_timing_t timing = {
        .zero_high   = LED_CYCLES(LED_T0H_NS),
        .zero_period = LED_CYCLES(LED_T0_BIT_NS),
        .one_high    = LED_CYCLES(LED_T1H_NS),
        .one_period  = LED_CYCLES(LED_T1_BIT_NS),
    };
    uint32_t saved_ps;

    // No clock line means one interrupt mid-frame corrupts the data, so mask
    // everything below the exception level until the frame is out.
    __asm__ __volatile__("rsil %0, 3" : "=a"(saved_ps) :: "memory");

    for (uint32_t i = 0; i < pixel_count; i++) {
        led_send_byte(pixels[i].green, &timing);     // GRB on the wire
        led_send_byte(pixels[i].red,   &timing);
        led_send_byte(pixels[i].blue,  &timing);
    }

    __asm__ __volatile__("wsr.ps %0; rsync" :: "a"(saved_ps) : "memory");

    gpio_set_low(led_pin);
    delay_us(LED_RESET_US);         // latch the frame
}

void led_set_color(led_color_t color)
{
    led_write(&color, 1);
}
