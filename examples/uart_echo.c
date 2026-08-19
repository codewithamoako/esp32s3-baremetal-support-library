/*
 * UART: a loopback self-test, then an echo server.
 *
 *   make uart_echo flash monitor
 *
 * Wiring - one jumper wire, nothing else:
 *
 *   GPIO5 (TX) ---- GPIO6 (RX)
 *
 * With that wire in place the port is talking to itself, which is enough to
 * prove the baud rate, the pin routing and both FIFOs without a second device
 * on the bench. Take the wire out and connect a real device to GPIO5/GPIO6
 * instead: the self-test then fails (nothing answers), and the echo loop
 * sends back whatever the device sends, which is a useful way to watch a
 * chatty module like a GPS.
 *
 * Results appear on the USB console, not on the UART, so the two are never
 * confused with each other. Watch them with `make monitor`.
 *
 * Note the two different serial ports in play here. console_print() goes out
 * of the USB-C socket to your PC; uart_print() goes out of a pin to whatever
 * is wired to it. They have nothing to do with each other.
 */

#include "esp32s3.h"
#include "board_pins.h"

#define UART_PORT   1
#define PIN_TX      PIN_GPIO5
#define PIN_RX      PIN_GPIO6
#define BAUD        115200

static const char probe[] = "loopback";

// Sends a known string and checks it comes back byte for byte.
static int loopback_ok(void)
{
    char got[sizeof probe];
    uint32_t want = sizeof probe - 1;      // the string without its terminator

    uart_print(UART_PORT, probe);
    uart_drain(UART_PORT);                 // wait for the last bit to leave TX

    // The bytes are already on their way back through the wire, but the
    // receiver still has to shift the last one in. At 115200 baud eight bytes
    // take about 0.7 ms, so this is a generous margin.
    delay_ms(10);

    if (uart_rx_available(UART_PORT) != want) {
        return 0;
    }

    uart_read(UART_PORT, got, want);
    for (uint32_t i = 0; i < want; i++) {
        if (got[i] != probe[i]) {
            return 0;
        }
    }
    return 1;
}

void _start(void)
{
    board_init();

    uart_init(UART_PORT, PIN_TX, PIN_RX, BAUD);

    console_print("uart port ");
    console_print_u32(UART_PORT);
    console_print(" on tx=");
    console_print_u32(PIN_TX);
    console_print(" rx=");
    console_print_u32(PIN_RX);
    console_print(" at ");
    console_print_u32(BAUD);
    console_print(" baud\r\n");

    if (loopback_ok()) {
        console_print("loopback ok - tx and rx agree\r\n");
    } else {
        console_print("loopback failed - no jumper, or a device is attached\r\n");
    }

    console_print("echoing anything that arrives on rx\r\n");

    // Nothing is buffered in RAM behind our back, so this loop has to come
    // round often enough to keep the 128-byte hardware FIFO from filling. At
    // this baud rate that is roughly every 11 ms.
    for (;;) {
        int c = uart_read_byte(UART_PORT);
        if (c < 0) {
            continue;                      // nothing waiting
        }

        uart_write_byte(UART_PORT, (uint8_t)c);

        console_print("rx 0x");
        console_print_hex((uint32_t)c, 2);
        if (c >= 0x20 && c < 0x7F) {
            console_print(" '");
            console_print_char((char)c);
            console_print("'");
        }
        console_print("\r\n");
    }
}
