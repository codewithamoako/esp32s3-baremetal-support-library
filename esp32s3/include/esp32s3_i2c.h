/*
 * I2C master - two wires, many devices, 7-bit addresses.
 *
 * The ESP32-S3's I2C controller is not a shift register you feed a byte at a
 * time. You hand it a short *program* - restart, write four bytes, restart,
 * read two, stop - drop the outgoing bytes in a FIFO, and it runs the whole
 * transaction on its own and tells you how it went. That is why the calls
 * here are whole transactions rather than start/byte/byte/stop primitives:
 * it is the shape the hardware actually has.
 *
 *     i2c_init(0, 8, 9, 400000);            // SDA on 8, SCL on 9, 400 kHz
 *
 *     uint8_t who;
 *     i2c_write_read(0, 0x68, (uint8_t[]){ 0x75 }, 1, &who, 1);
 *
 * Both pins need a pull-up to 3V3 - 4.7k is the usual choice. The internal
 * pull-ups this driver switches on are around 45k, which is weak enough that
 * they only work on a short bus at 100 kHz. Treat them as a convenience for
 * poking at a sensor on a breadboard, not as the real thing.
 *
 * One transaction moves at most 32 bytes in each direction, because that is
 * the depth of the hardware FIFO and this driver does not refill it midway.
 * Longer transfers return I2C_ERR_TOO_LONG rather than silently truncating.
 * Registers, IDs and sensor readings all fit comfortably; a display's frame
 * buffer does not, and wants splitting into several writes.
 *
 * The bus clock comes from the 40 MHz crystal, so it survives a CPU clock
 * change. Every call is polled and returns when the transaction is over.
 */

#ifndef ESP32S3_I2C_H
#define ESP32S3_I2C_H

#include <stdint.h>

// How a transaction ended. Anything but I2C_OK means no data moved.
typedef enum {
    I2C_OK            =  0,
    I2C_ERR_NACK      = -1,   // nobody answered, or a device refused a byte
    I2C_ERR_TIMEOUT   = -2,   // a device stretched the clock past the limit
    I2C_ERR_ARBITRATION = -3, // another master was talking at the same time
    I2C_ERR_TOO_LONG  = -4,   // more than the 32 bytes one transaction holds
    I2C_ERR_ARG       = -5,   // bad port number
} i2c_status_t;

// The most bytes one call can move in each direction.
#define I2C_MAX_TRANSFER  32

// Sets a port up as a master at the given bus speed and puts it on two pins.
// Switches the internal pull-ups on; add real ones for anything serious.
// - port: 0 or 1
// - sda_pin: GPIO for the data line
// - scl_pin: GPIO for the clock line
// - hz: bus speed, typically 100000 or 400000. Clamped to 1 kHz..1 MHz,
//       which is the range the bus standard and the timing registers share
// returns: nothing
void i2c_init(uint32_t port, uint32_t sda_pin, uint32_t scl_pin, uint32_t hz);

// Writes bytes to a device: START, address, data, STOP.
// - port: 0 or 1
// - addr: the device's 7-bit address, unshifted (e.g. 0x68, not 0xD0)
// - data: the bytes to send
// - len: how many, up to 31 (the address takes one of the 32 FIFO slots)
// returns: I2C_OK, or the reason it failed
i2c_status_t i2c_write(uint32_t port, uint8_t addr, const void *data, uint32_t len);

// Reads bytes from a device: START, address, data, STOP. The last byte read
// is NACKed, which is how a master tells a device to stop sending.
// - port: 0 or 1
// - addr: the device's 7-bit address, unshifted
// - buf: where to put the bytes
// - len: how many to read, 1 to 32
// returns: I2C_OK, or the reason it failed
i2c_status_t i2c_read(uint32_t port, uint8_t addr, void *buf, uint32_t len);

// Writes, then reads back without letting go of the bus - a repeated START
// rather than a STOP in between. This is how nearly every device's "read
// register N" works, and doing it as two separate calls can lose the bus to
// another master in the gap.
// - port: 0 or 1
// - addr: the device's 7-bit address, unshifted
// - tx: the bytes to send first, usually a register number
// - tx_len: how many, up to 31
// - rx: where to put what comes back
// - rx_len: how many to read, 1 to 32
// returns: I2C_OK, or the reason it failed
i2c_status_t i2c_write_read(uint32_t port, uint8_t addr,
                            const void *tx, uint32_t tx_len,
                            void *rx, uint32_t rx_len);

// Addresses a device and stops, to find out whether anything is there.
// - port: 0 or 1
// - addr: the 7-bit address to try
// returns: I2C_OK if a device acknowledged, I2C_ERR_NACK if none did
i2c_status_t i2c_probe(uint32_t port, uint8_t addr);

#endif // ESP32S3_I2C_H
