/*
 * Waveshare ESP32-S3-Zero pin definitions.
 *
 * Every GPIO the board breaks out, plus the onboard peripherals, named so
 * that a program reads like the silkscreen instead of a bare number.
 *
 * Source: https://docs.waveshare.com/ESP32-S3-Zero
 *
 * Board facts worth knowing:
 *   - The chip is an ESP32-S3FH4R2 (dual-core, 4 MB flash, 2 MB PSRAM).
 *   - GPIO21 drives the onboard WS2812 RGB LED.
 *   - GPIO0 is the BOOT button (hold + reset to enter download mode).
 *   - The "TX"/"RX" silkscreen labels are UART0: TX = GPIO43, RX = GPIO44.
 *   - The native USB port (USB-Serial-JTAG) sits on GPIO19/GPIO20.
 *   - GPIO33..GPIO37 are NOT broken out - they are reserved for the octal
 *     PSRAM interface and must not be used.
 *   - The 5V pad is the external supply input (3.7V-6V); 3V3 is the onboard
 *     regulated rail.
 */

#ifndef BOARD_PINS_H
#define BOARD_PINS_H

// Power and ground - not GPIOs, listed for wiring reference.
//   PIN_VIN  : 5V silkscreen pad - external supply in, 3.7V-6V
//   PIN_3V3  : 3.3V regulated rail (from the onboard ME6217 LDO)
//   PIN_GND  : ground

// Onboard peripherals
#define PIN_LED           21     // the board's only LED: a WS2812
#define PIN_BUTTON_BOOT    0     // BOOT / download-mode button
#define PIN_USB_DN        19     // USB-OTG D- (also the USB console)
#define PIN_USB_DP        20     // USB-OTG D+ (also the USB console)
#define PIN_UART0_TX      43     // "TX" silkscreen
#define PIN_UART0_RX      44     // "RX" silkscreen

// General-purpose GPIOs broken out on the castellated pads
#define PIN_GPIO0   0
#define PIN_GPIO1   1
#define PIN_GPIO2   2
#define PIN_GPIO3   3
#define PIN_GPIO4   4
#define PIN_GPIO5   5
#define PIN_GPIO6   6
#define PIN_GPIO7   7
#define PIN_GPIO8   8
#define PIN_GPIO9   9
#define PIN_GPIO10  10
#define PIN_GPIO11  11
#define PIN_GPIO12  12
#define PIN_GPIO13  13
#define PIN_GPIO14  14
#define PIN_GPIO15  15
#define PIN_GPIO16  16
#define PIN_GPIO17  17
#define PIN_GPIO18  18
#define PIN_GPIO19  19     // shared with USB D-
#define PIN_GPIO20  20     // shared with USB D+
#define PIN_GPIO21  21     // shared with the WS2812 LED

// Pins the chip has but this board does NOT break out. Use them and they
// simply are not reachable on the castellated pads.
//   GPIO33..GPIO37 : reserved for the octal PSRAM interface.

#endif // BOARD_PINS_H
