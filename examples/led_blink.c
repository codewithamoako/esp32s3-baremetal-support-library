/*
 * LED: cycle the onboard RGB LED through red, green, blue, and off.
 *
 *   make led_blink flash monitor
 *
 * No wiring at all - the RGB LED is already on the board.
 */

#include "esp32s3.h"
#include "board_pins.h"
#include "led.h"


void _start(void)
{
    board_init();

    // The onboard LED is a three-channel RGB device addressed through one pin.
    led_init(PIN_LED);

    while(1)
    {
        // Hold each color for half a second before moving to the next one.
        led_set_color(LED_RED);
        delay_ms(500);

        led_set_color(LED_GREEN);
        delay_ms(500);

        led_set_color(LED_BLUE);
        delay_ms(500);

        led_set_color(LED_OFF);
        delay_ms(500);
    }

}
