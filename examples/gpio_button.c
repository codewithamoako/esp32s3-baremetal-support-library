/*
 * GPIO: read the onboard BOOT button and show its state on the RGB LED.
 *
 *   make gpio_button flash monitor
 *
 * No wiring at all - both the button and the LED are already on the board.
 *
 * The BOOT button shorts GPIO0 to ground when pressed, so the pin reads 0
 * while held and needs a pull-up to read 1 the rest of the time. That is the
 * usual way buttons are wired, and it is why "pressed" is a low: a switch can
 * connect a pin to something, but it cannot hold it anywhere on its own.
 *
 * Holding BOOT during a reset also puts the chip in download mode, which is
 * how you recover a board that will not take a flash. Pressing it while the
 * program runs, as here, is harmless.
 *
 * The debounce below matters more than it looks. A switch's contacts bounce
 * apart and back together for a millisecond or two after they meet, so a loop
 * reading the pin fast enough will see one press as a burst of presses. This
 * only accepts a change that is still there a moment later.
 */

#include "esp32s3.h"
#include "led.h"
#include "board_pins.h"

#define DEBOUNCE_MS     20

// The button pulls the pin down, so a low level is a press.
static int button_pressed(void)
{
    return gpio_read(PIN_BUTTON_BOOT) == 0;
}

void _start(void)
{
    board_init();

    // Pulled up, because nothing drives this pin while the button is open.
    gpio_config_input(PIN_BUTTON_BOOT, GPIO_PULLUP);
    led_init(PIN_LED);



    int state = button_pressed();
    uint32_t presses = 0;

    led_set_color(state ? LED_GREEN : LED_OFF );
        console_print("press BOOT (green = up, red = down)\r\n");

    for (;;) {
        int now = button_pressed();

        if (now == state) {
            continue;
        }

        // Something changed - wait out the contact bounce and look again. If
        // it was a bounce rather than a real press, the level has gone back
        // to where it was and there is nothing to report.
        delay_ms(DEBOUNCE_MS);
        if (button_pressed() != now) {
            continue;
        }

        state = now;
        led_set_color(state ? LED_GREEN : LED_OFF );

        if (state) {
            presses++;
            console_print("press ");
            console_print_u32(presses);
            console_print("\r\n");
        }
    }
}
