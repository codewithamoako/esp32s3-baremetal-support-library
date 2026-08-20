/*
 * SPI master on GP-SPI2, CPU-driven through the 64-byte data buffer.
 */

#include "esp32s3_spi.h"
#include "esp32s3_clock.h"
#include "esp32s3_gpio.h"
#include "esp32s3_regs.h"

// The controller's own clock, selected in spi_init(). Fixed at 80 MHz off the
// PLL, which is why the bit rate does not move when the CPU clock does.
#define SPI_SRC_HZ      80000000u

// W0..W15 hold one transfer, and this driver does not use DMA.
#define SPI_CHUNK_BYTES (SPI_BUF_WORDS * 4)

#define SPI_REG(off)    ESP32S3_REG(SPI2_BASE + (off))

static int spi_has_cs;      // whether the hardware drives chip select for us

// The bit clock is the source divided twice: a pre-divider of 1..16, then a
// counter of 2..64 that also shapes the high and low halves of each tick.
// Every combination is tried, and the fastest rate that does not exceed what
// was asked for wins - overshooting a device's rated clock is the one error
// here that shows up as rare corrupt bytes rather than as nothing working.
static void set_clock(uint32_t hz)
{
    if (hz >= SPI_SRC_HZ) {
        SPI_REG(SPI_CLOCK_OFF) = SPI_CLK_EQU_SYSCLK;    // bypass both dividers
        return;
    }

    uint32_t best_pre = 16, best_n = 64, best_hz = 0;

    for (uint32_t pre = 1; pre <= 16; pre++) {
        for (uint32_t n = 2; n <= 64; n++) {
            uint32_t f = SPI_SRC_HZ / (pre * n);
            if (f <= hz && f > best_hz) {
                best_hz  = f;
                best_pre = pre;
                best_n   = n;
            }
        }
    }

    // Low for the first half of the tick, high for the rest.
    SPI_REG(SPI_CLOCK_OFF) =
          ((best_n - 1) << SPI_CLKCNT_L_S)
        | ((best_n / 2 - 1) << SPI_CLKCNT_H_S)
        | ((best_n - 1) << SPI_CLKCNT_N_S)
        | ((best_pre - 1) << SPI_CLKDIV_PRE_S);
}

void spi_init(uint32_t sck_pin, uint32_t mosi_pin, uint32_t miso_pin,
              uint32_t cs_pin, uint32_t hz, uint32_t mode)
{
    periph_enable(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_PERIP_RST_EN0_REG, SYSTEM_SPI2_CLK_EN);

    SPI_REG(SPI_CLK_GATE_OFF) = SPI_CLK_GATE_EN | SPI_MST_CLK_ACTIVE | SPI_MST_CLK_SEL;
    SPI_REG(SPI_SLAVE_OFF)    = 0;                  // master
    SPI_REG(SPI_DMA_CONF_OFF) = 0;

    set_clock(hz);

    // Full duplex, and no command, address or dummy phase - just data. Those
    // phases exist for flash chips that want an opcode sent a particular way;
    // an ordinary device gets its opcode as the first data byte instead.
    uint32_t user = SPI_DOUTDIN | SPI_USR_MOSI | SPI_USR_MISO;
    uint32_t misc = SPI_CS0_DIS | SPI_CS1_DIS | SPI_CS2_DIS;

    // The two halves of the mode number. CK_IDLE_EDGE is the clock's resting
    // level; CK_OUT_EDGE picks which edge of each bit the data changes on,
    // and it is deliberately not a straight copy of the phase bit - modes 0
    // and 3 want one setting, modes 1 and 2 the other.
    if (mode == 2 || mode == 3) {
        misc |= SPI_CK_IDLE_EDGE;
    }
    if (mode == 1 || mode == 2) {
        user |= SPI_CK_OUT_EDGE;
    }

    SPI_REG(SPI_USER1_OFF) = 0;
    SPI_REG(SPI_USER2_OFF) = 0;

    spi_has_cs = (cs_pin != SPI_PIN_NONE);
    if (spi_has_cs) {
        misc &= ~SPI_CS0_DIS;
        gpio_route_out(cs_pin, SPI2_CS0_SIG);
    }

    SPI_REG(SPI_USER_OFF) = user;
    SPI_REG(SPI_MISC_OFF) = misc;

    gpio_route_out(sck_pin, SPI2_CLK_SIG);
    if (mosi_pin != SPI_PIN_NONE) {
        gpio_route_out(mosi_pin, SPI2_MOSI_SIG);
    }
    if (miso_pin != SPI_PIN_NONE) {
        gpio_route_in(miso_pin, SPI2_MISO_SIG, GPIO_FLOAT);
    }

    SPI_REG(SPI_CMD_OFF) = SPI_UPDATE;
    while (SPI_REG(SPI_CMD_OFF) & SPI_UPDATE) { }
}

// One transfer of up to 64 bytes. keep_cs holds the chip select down
// afterwards so a longer transfer reads as a single transaction to the device.
static void transfer_chunk(const uint8_t *tx, uint8_t *rx, uint32_t len, int keep_cs)
{
    // The data buffer is sixteen 32-bit registers, not a byte array, so the
    // bytes are packed in and unpacked out a word at a time. Unsent lanes of
    // the last word are don't-care; the bit length is what ends the transfer.
    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = 0;
        for (uint32_t b = 0; b < 4 && i + b < len; b++) {
            word |= (uint32_t)(tx ? tx[i + b] : 0u) << (8 * b);
        }
        SPI_REG(SPI_W_OFF(i / 4)) = word;
    }

    SPI_REG(SPI_MS_DLEN_OFF) = len * 8 - 1;

    uint32_t misc = SPI_REG(SPI_MISC_OFF);
    if (keep_cs) {
        misc |= SPI_CS_KEEP_ACTIVE;
    } else {
        misc &= ~SPI_CS_KEEP_ACTIVE;
    }
    SPI_REG(SPI_MISC_OFF) = misc;

    // Most of these registers are read on the SPI clock rather than the CPU's,
    // so the writes above only take effect once they are handed across.
    SPI_REG(SPI_CMD_OFF) = SPI_UPDATE;
    while (SPI_REG(SPI_CMD_OFF) & SPI_UPDATE) { }

    SPI_REG(SPI_CMD_OFF) = SPI_USR;
    while (SPI_REG(SPI_CMD_OFF) & SPI_USR) { }

    if (rx) {
        for (uint32_t i = 0; i < len; i += 4) {
            uint32_t word = SPI_REG(SPI_W_OFF(i / 4));
            for (uint32_t b = 0; b < 4 && i + b < len; b++) {
                rx[i + b] = (uint8_t)(word >> (8 * b));
            }
        }
    }
}

void spi_transfer(const void *tx, void *rx, uint32_t len)
{
    const uint8_t *out = tx;
    uint8_t *in = rx;

    while (len > 0) {
        uint32_t chunk = len > SPI_CHUNK_BYTES ? SPI_CHUNK_BYTES : len;
        len -= chunk;

        // Hold the chip select down over every seam but the last one. With no
        // hardware chip select there is nothing to hold, and the caller is
        // driving the pin across the whole transfer anyway.
        transfer_chunk(out, in, chunk, spi_has_cs && len > 0);

        if (out) {
            out += chunk;
        }
        if (in) {
            in += chunk;
        }
    }
}

void spi_write(const void *data, uint32_t len)
{
    spi_transfer(data, 0, len);
}

void spi_read(void *buf, uint32_t len)
{
    spi_transfer(0, buf, len);
}

uint8_t spi_transfer_byte(uint8_t byte)
{
    uint8_t received = 0;

    spi_transfer(&byte, &received, 1);
    return received;
}
