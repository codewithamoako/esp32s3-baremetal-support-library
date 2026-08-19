/*
 * SPI: a loopback self-test that also exercises the multi-chunk path.
 *
 *   make spi_loopback flash monitor
 *
 * Wiring - one jumper wire:
 *
 *   GPIO11 (MOSI) ---- GPIO13 (MISO)
 *
 * SPI is full duplex: the same clock edge that shifts a bit out of MOSI
 * shifts one in on MISO. Tie those two pins together and every byte sent
 * arrives back in the same transfer, so a mismatch means the bus is wrong
 * rather than that some device misbehaved. That makes this the cheapest way
 * to prove out clock polarity, bit order and the pin routing.
 *
 * The second test sends 200 bytes, which is more than the 64 the hardware
 * moves at once. The driver splits it and holds chip select down across the
 * seams, so a real device would see one unbroken transaction - this checks
 * that no byte is dropped or duplicated at the joins.
 *
 * To talk to a real device instead, take the wire out and use the JEDEC ID
 * read at the bottom of this file as a starting point.
 */

#include "esp32s3.h"
#include "board_pins.h"

#define PIN_SCK     PIN_GPIO12
#define PIN_MOSI    PIN_GPIO11
#define PIN_MISO    PIN_GPIO13
#define PIN_CS      PIN_GPIO10
#define SPI_HZ      1000000     // gentle enough for a breadboard jumper
#define SPI_MODE    0

#define LONG_LEN    200         // more than one 64-byte hardware transfer

static uint8_t sent[LONG_LEN];
static uint8_t got[LONG_LEN];

// Reports pass or fail and returns whether it passed.
static int check(const char *name, const uint8_t *a, const uint8_t *b, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            console_print("FAIL ");
            console_print(name);
            console_print(" at byte ");
            console_print_u32(i);
            console_print(": sent 0x");
            console_print_hex(a[i], 2);
            console_print(", got 0x");
            console_print_hex(b[i], 2);
            console_print("\r\n");
            return 0;
        }
    }

    console_print("ok   ");
    console_print(name);
    console_print(" (");
    console_print_u32(len);
    console_print(" bytes)\r\n");
    return 1;
}

void _start(void)
{
    board_init();

    spi_init(PIN_SCK, PIN_MOSI, PIN_MISO, PIN_CS, SPI_HZ, SPI_MODE);

    console_print("spi2 sck=");
    console_print_u32(PIN_SCK);
    console_print(" mosi=");
    console_print_u32(PIN_MOSI);
    console_print(" miso=");
    console_print_u32(PIN_MISO);
    console_print(" cs=");
    console_print_u32(PIN_CS);
    console_print(" mode ");
    console_print_u32(SPI_MODE);
    console_print("\r\n");

    // A pattern where every byte differs from its neighbours, so a transfer
    // that is off by one byte cannot accidentally still match.
    for (uint32_t i = 0; i < LONG_LEN; i++) {
        sent[i] = (uint8_t)(i * 7u + 1u);
    }

    for (;;) {
        // One byte, the shortest transfer there is.
        uint8_t one = spi_transfer_byte(0xA5);
        console_print(one == 0xA5 ? "ok   single byte\r\n"
                                  : "FAIL single byte - is the jumper on?\r\n");

        // A single hardware transfer, exactly filling the 64-byte buffer.
        spi_transfer(sent, got, 64);
        check("one full chunk", sent, got, 64);

        // Longer than the buffer, so the driver splits it internally.
        spi_transfer(sent, got, LONG_LEN);
        check("split transfer", sent, got, LONG_LEN);

        console_print("\r\n");
        delay_ms(2000);
    }
}

/*
 * Reading a SPI flash chip's JEDEC ID, for when a real device is wired up
 * instead of the jumper. Command 0x9F, then three bytes back: manufacturer,
 * then two of device type. A Winbond W25Q32 answers EF 40 16.
 *
 *     uint8_t id[3];
 *     spi_write((const uint8_t[]){ 0x9F }, 1);
 *     spi_read(id, 3);
 *
 * That is two calls, so chip select rises in between - which most flash chips
 * will not accept, since the command and its answer have to be one unbroken
 * transaction. Do it as a single full-duplex transfer instead, and ignore the
 * first byte coming back, which arrives while the command is still going out:
 *
 *     uint8_t tx[4] = { 0x9F, 0, 0, 0 };
 *     uint8_t rx[4];
 *     spi_transfer(tx, rx, 4);
 *     // rx[1], rx[2], rx[3] are the ID
 */
