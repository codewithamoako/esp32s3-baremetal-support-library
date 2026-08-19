/*
 * SPI master on GP-SPI2 - clock, two data lines, and a chip select.
 *
 * SPI is full duplex by nature: every clock edge shifts a bit out and a bit
 * in at the same time, so a transfer of n bytes always moves n bytes in both
 * directions whether or not you care about both. spi_transfer() is the honest
 * form of that; spi_write() and spi_read() are the same call with one side
 * thrown away.
 *
 *     spi_init(12, 11, 13, 10, 10000000, 0);   // SCK, MOSI, MISO, CS, 10 MHz
 *
 *     uint8_t cmd[] = { 0x9F }, id[3];
 *     gpio_set_low(10);                  // if you drive CS yourself
 *     spi_write(cmd, 1);
 *     spi_read(id, 3);
 *
 * One bus, one device. The controller has three chip-select outputs but this
 * driver wires up only the first, because juggling several devices means
 * changing clock and mode between them, and that belongs in whatever code
 * knows what those devices are. For a second device, pass SPI_PIN_NONE for
 * the chip select and drive the pins yourself with gpio_write().
 *
 * Transfers of any length work, but the hardware moves 64 bytes at a time -
 * that is the size of its data buffer, and this driver does not use DMA.
 * Longer transfers are split, with the chip select held down across the seam
 * so the device sees one unbroken transaction.
 *
 * The bit clock comes from the 80 MHz PLL, so it does not shift when the CPU
 * clock does. A request lands on the nearest rate at or below what you asked
 * for, never above - overshooting a device's rated clock is the failure that
 * shows up as rare corrupt bytes rather than as nothing working at all. The
 * dividers reach down to 78125 Hz and no further, so a request below that
 * gets 78125 Hz, the one case where the rate comes out higher than asked.
 */

#ifndef ESP32S3_SPI_H
#define ESP32S3_SPI_H

#include <stdint.h>

// Pass instead of a pin number to leave that signal unwired.
#define SPI_PIN_NONE    0xFFFFFFFFu

// Sets the bus up and puts it on four pins.
//
// The mode is the usual SPI 0-3, which is two independent choices written as
// one number: whether the clock idles low (0, 1) or high (2, 3), and whether
// data is sampled on the first edge of each bit (0, 2) or the second (1, 3).
// Mode 0 is what most devices want; the datasheet will say.
//
// - sck_pin: GPIO for the clock
// - mosi_pin: GPIO for data out, or SPI_PIN_NONE
// - miso_pin: GPIO for data in, or SPI_PIN_NONE
// - cs_pin: GPIO for chip select, or SPI_PIN_NONE to drive it yourself
// - hz: bit clock, e.g. 10000000
// - mode: 0, 1, 2 or 3
// returns: nothing
void spi_init(uint32_t sck_pin, uint32_t mosi_pin, uint32_t miso_pin,
              uint32_t cs_pin, uint32_t hz, uint32_t mode);

// Sends and receives at the same time, which is what the wires do anyway.
// - tx: bytes to send, or 0 to send zeros
// - rx: where to put what arrives, or 0 to discard it
// - len: how many bytes, any length
// returns: nothing
void spi_transfer(const void *tx, void *rx, uint32_t len);

// Sends bytes and discards what comes back.
// - data: the bytes to send
// - len: how many
// returns: nothing
void spi_write(const void *data, uint32_t len);

// Reads bytes, sending zeros to clock them out.
// - buf: where to put them
// - len: how many
// returns: nothing
void spi_read(void *buf, uint32_t len);

// Sends one byte and returns the one that arrived in its place.
// - byte: the byte to send
// returns: the byte received
uint8_t spi_transfer_byte(uint8_t byte);

#endif // ESP32S3_SPI_H
