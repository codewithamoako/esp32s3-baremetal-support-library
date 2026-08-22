/*
 * I2C: scan the bus for devices, then read a register from each one found.
 *
 *   make i2c_scan flash monitor
 *
 * Wiring:
 *
 *   GPIO8  ---- SDA ---- 4.7k ---- 3V3
 *   GPIO9  ---- SCL ---- 4.7k ---- 3V3
 *   GND    ---- GND
 *
 * The driver switches the chip's internal pull-ups on, which is often just
 * enough to get a single sensor answering on a breadboard at 100 kHz. Real
 * resistors are what make it reliable, and are not optional at 400 kHz or
 * with more than one device on the wire.
 *
 * A scan is the first thing to run against any I2C device, because it settles
 * the question that wastes the most time: is the thing wired up correctly and
 * at the address the datasheet claims? Addresses are given as 7-bit here.
 * Datasheets often print the 8-bit form instead, which is this number doubled
 * - a device listed as 0xD0/0xD1 is 0x68 to this driver.
 */

#include "board.h"
#include "board_pins.h"

#define I2C_PORT    0
#define PIN_SDA     PIN_GPIO8
#define PIN_SCL     PIN_GPIO9
#define BUS_HZ      100000

// 0x00-0x07 and 0x78-0x7F are reserved by the I2C standard for things like
// the general call and 10-bit addressing, so no ordinary device lives there.
#define ADDR_FIRST  0x08
#define ADDR_LAST   0x77

static void report(uint8_t addr)
{
    console_print("  found 0x");
    console_print_hex(addr, 2);

    // Register 0 is not meaningful on every device, but reading it proves the
    // whole write-then-read turnaround works and not just the address phase.
    uint8_t reg0 = 0;
    i2c_status_t status = i2c_write_read(I2C_PORT, addr, (const uint8_t[]){ 0x00 }, 1, &reg0, 1);

    if (status == I2C_OK) {
        console_print("  reg[0] = 0x");
        console_print_hex(reg0, 2);
    } else {
        console_print("  (addressed, but would not be read from)");
    }
    console_print("\r\n");
}

void _start(void)
{
    board_init();

    i2c_init(I2C_PORT, PIN_SDA, PIN_SCL, BUS_HZ);

    console_print("i2c port ");
    console_print_u32(I2C_PORT);
    console_print(" on sda=");
    console_print_u32(PIN_SDA);
    console_print(" scl=");
    console_print_u32(PIN_SCL);
    console_print(" at ");
    console_print_u32(BUS_HZ / 1000);
    console_print(" kHz\r\n");

    for (;;) {
        console_print("scanning 0x08..0x77\r\n");

        uint32_t found = 0;
        for (uint8_t addr = ADDR_FIRST; addr <= ADDR_LAST; addr++) {
            // A probe is a zero-length write: the address goes out and the
            // controller reports whether anything pulled SDA down to answer.
            if (i2c_probe(I2C_PORT, addr) == I2C_OK) {
                found++;
                report(addr);
            }
        }

        console_print_u32(found);
        console_print(found == 1 ? " device\r\n\r\n" : " devices\r\n\r\n");

        delay_ms(3000);
    }
}
