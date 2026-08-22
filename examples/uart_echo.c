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
 *
 * The self-test repeats rather than running once at startup, and that is not
 * cosmetic. Resetting this board tears the USB console down and re-enumerates
 * it, which takes a few hundred milliseconds; anything printed before the
 * host comes back is written into a FIFO nobody is draining and dropped. A
 * one-shot banner is therefore never seen - `make monitor` always attaches
 * long after it has been thrown away, and the screen stays blank forever.
 * Repeating it means that whenever you start watching, the next report is a
 * couple of seconds away. It stops once real bytes start arriving, so echoed
 * traffic is not interleaved with a banner you have already read.
 */

#include "board.h"
#include "board_pins.h"

#define UART_PORT   1
#define PIN_TX      PIN_GPIO5
#define PIN_RX      PIN_GPIO6
#define BAUD        115200

#define REPORT_MS   2000

static const char probe[] = "loopback";

// Sends a known string and checks it comes back byte for byte.
static int loopback_ok(void)
{
    char got[sizeof probe];
    uint32_t want = sizeof probe - 1;      // the string without its terminator

    // Start from an empty receiver: on a repeat run the previous attempt may
    // have left a partial reply behind, and that would fail the count below
    // for the wrong reason.
    while (uart_read_byte(UART_PORT) >= 0) { }

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

static void report(void)
{
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
}

void _start(void)
{
    board_init();

    uart_init(UART_PORT, PIN_TX, PIN_RX, BAUD);

    // cycle_count() rather than a delay: the echo loop below has to keep
    // coming round, so the report cannot be scheduled by blocking on one.
    uint32_t period = cpu_mhz() * 1000u * REPORT_MS;
    uint32_t last   = cycle_count() - period;   // report on the first pass
    int      quiet  = 1;                        // nothing has arrived yet

    // Nothing is buffered in RAM behind our back, so this loop has to come
    // round often enough to keep the 128-byte hardware FIFO from filling. At
    // this baud rate that is roughly every 11 ms.
    for (;;) {
        // Empty the FIFO rather than taking one byte per pass: a burst can
        // arrive faster than a pass that also prints can consume it.
        int c;
        while ((c = uart_read_byte(UART_PORT)) >= 0) {
            quiet = 0;
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

        // Unsigned subtraction, so the 32-bit cycle counter wrapping - every
        // 27 seconds at 160 MHz - costs at most one late report.
        if (quiet && cycle_count() - last >= period) {
            last = cycle_count();
            report();
        }
    }
}
