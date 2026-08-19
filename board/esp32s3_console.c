/*
 * Text output over the chip's built-in USB-Serial-JTAG peripheral.
 */

#include "esp32s3_console.h"
#include "esp32s3_regs.h"

// How many polls to spend waiting for FIFO space before giving up. Counted in
// loop passes, not time, so this costs nothing when a host is connected.
#define CONSOLE_FIFO_SPIN_LIMIT 200000u

void console_print_char(char c)
{
    // With no host reading the port the FIFO never drains, so give up rather
    // than stall the caller forever.
    for (uint32_t spin = 0; spin < CONSOLE_FIFO_SPIN_LIMIT; spin++) {
        if (ESP32S3_REG(USB_SERIAL_JTAG_EP1_CONF_REG) & USB_SERIAL_JTAG_IN_EP_DATA_FREE) {
            ESP32S3_REG(USB_SERIAL_JTAG_EP1_REG)      = (uint8_t)c;
            ESP32S3_REG(USB_SERIAL_JTAG_EP1_CONF_REG) = USB_SERIAL_JTAG_WR_DONE;  // ship it
            return;
        }
    }
}

void console_print(const char *text)
{
    while (*text) {
        console_print_char(*text++);
    }
}

void console_print_u32(uint32_t value)
{
    char digits[10];        // 4294967295 is the longest a uint32_t gets
    int  count = 0;

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);

    while (count--) {
        console_print_char(digits[count]);
    }
}

void console_print_hex(uint32_t value, uint32_t digits)
{
    if (digits < 1) {
        digits = 1;
    } else if (digits > 8) {
        digits = 8;
    }

    // Most significant nibble first, so the shift counts down.
    while (digits--) {
        uint32_t nibble = (value >> (4 * digits)) & 0xFu;
        console_print_char((char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10));
    }
}
