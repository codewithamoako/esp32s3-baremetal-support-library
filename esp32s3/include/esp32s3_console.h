/*
 * Text output over USB-Serial-JTAG.
 *
 * The board's single USB-C port is the chip's built-in USB-Serial-JTAG
 * peripheral, which the ROM has already enumerated - so printing costs two
 * registers, no driver, no UART and no pin. Output appears on the same COM
 * port you flash over.
 *
 * Writes are dropped rather than blocked when no host is reading the port, so
 * an unattended board keeps running at full speed.
 */

#ifndef ESP32S3_CONSOLE_H
#define ESP32S3_CONSOLE_H

#include <stdint.h>

// Prints a single character to the USB console.
// - c: the character to print
// returns: nothing
void console_print_char(char c);

// Prints a null-terminated string to the USB console.
// - text: the string to print
// returns: nothing
void console_print(const char *text);

// Prints an unsigned number in decimal to the USB console.
// - value: the number to print (0 to 4294967295)
// returns: nothing
void console_print_u32(uint32_t value);

// Prints a number in hexadecimal, zero-padded, without a "0x" prefix.
// Register values and I2C addresses are far easier to read this way.
// - value: the number to print
// - digits: how many hex digits to pad to, 1 to 8 (2 for a byte)
// returns: nothing
void console_print_hex(uint32_t value, uint32_t digits);

#endif // ESP32S3_CONSOLE_H
