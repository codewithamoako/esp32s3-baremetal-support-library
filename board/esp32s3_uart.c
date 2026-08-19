/*
 * Hardware UART: 8N1, polled, clocked from the crystal.
 */

#include "esp32s3_uart.h"
#include "esp32s3_clock.h"
#include "esp32s3_gpio.h"
#include "esp32s3_regs.h"

#define UART_PORTS  3

// The three ports are not evenly spaced in the address map, and UART2's clock
// bit lives in the second enable register rather than the first.
static const uint32_t uart_base[UART_PORTS] = { UART0_BASE, UART1_BASE, UART2_BASE };

static uint32_t reg_of(uint32_t port, uint32_t offset)
{
    return uart_base[port < UART_PORTS ? port : 0] + offset;
}

// The baud rate divider is a fraction: an integer part in whole source-clock
// ticks, a numerator in sixteenths, and a pre-divider ahead of both. Only the
// pre-divider makes the slow rates reachable - the integer part is 12 bits,
// which at 40 MHz runs out below about 9800 baud.
static void set_baud(uint32_t port, uint32_t baud)
{
    const uint32_t sclk = ESP32S3_XTAL_MHZ * 1000000u;

    uint32_t pre_div = 1;
    if (baud < sclk / UART_CLKDIV_M) {
        pre_div = (sclk / baud + UART_CLKDIV_M - 1) / UART_CLKDIV_M;
    }
    if (pre_div > UART_SCLK_DIV_NUM_M + 1) {
        pre_div = UART_SCLK_DIV_NUM_M + 1;      // slower than the hardware goes
    }

    uint32_t sixteenths = (sclk << 4) / (baud * pre_div);

    ESP32S3_REG(reg_of(port, UART_CLKDIV_OFF)) =
          ((sixteenths >> 4) & UART_CLKDIV_M)
        | ((sixteenths & 0xFu) << UART_CLKDIV_FRAG_S);

    uint32_t clk = ESP32S3_REG(reg_of(port, UART_CLK_CONF_OFF));
    clk &= ~(UART_SCLK_DIV_NUM_M << UART_SCLK_DIV_NUM_S);
    clk |= (pre_div - 1) << UART_SCLK_DIV_NUM_S;
    ESP32S3_REG(reg_of(port, UART_CLK_CONF_OFF)) = clk;
}

// Most of the UART's registers are read by a state machine on the UART's own
// clock, not the CPU's. This hands the pending writes across that boundary.
static void latch_config(uint32_t port)
{
    ESP32S3_REG(reg_of(port, UART_ID_OFF)) |= UART_REG_UPDATE;
    while (ESP32S3_REG(reg_of(port, UART_ID_OFF)) & UART_REG_UPDATE) { }
}

void uart_init(uint32_t port, uint32_t tx_pin, uint32_t rx_pin, uint32_t baud)
{
    if (port >= UART_PORTS || baud == 0) {
        return;
    }

    // UART2's bit sits in the second clock-enable register, the other two in
    // the first. The bit number differs as well, so both come from a pair.
    if (port == 2) {
        periph_enable(SYSTEM_PERIP_CLK_EN1_REG, SYSTEM_PERIP_RST_EN1_REG, SYSTEM_UART2_CLK_EN);
    } else {
        periph_enable(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_PERIP_RST_EN0_REG,
                      port == 0 ? SYSTEM_UART0_CLK_EN : SYSTEM_UART1_CLK_EN);
    }

    // Source the bit clock from the crystal. The alternative is an 80 MHz
    // clock off the PLL, which would make every baud rate depend on a clock
    // switch happening somewhere else in the program.
    ESP32S3_REG(reg_of(port, UART_CLK_CONF_OFF)) =
        (UART_SCLK_SEL_XTAL << UART_SCLK_SEL_S) | UART_SCLK_EN
        | UART_TX_SCLK_EN | UART_RX_SCLK_EN;

    // Take the transmit and receive state machines through reset now that
    // they have a clock to be reset on.
    ESP32S3_REG(reg_of(port, UART_CLK_CONF_OFF)) |= UART_RST_CORE;
    ESP32S3_REG(reg_of(port, UART_CLK_CONF_OFF)) &= ~UART_RST_CORE;

    set_baud(port, baud);

    // 8 data bits (3), one stop bit (1), no parity - and nothing else, which
    // is what a zeroed CONF0 gives us.
    ESP32S3_REG(reg_of(port, UART_CONF0_OFF)) =
        (3u << UART_BIT_NUM_S) | (1u << UART_STOP_BIT_NUM_S);

    // Empty both FIFOs of whatever the ROM or a previous run left behind.
    ESP32S3_REG(reg_of(port, UART_CONF0_OFF)) |= UART_RXFIFO_RST | UART_TXFIFO_RST;
    ESP32S3_REG(reg_of(port, UART_CONF0_OFF)) &= ~(UART_RXFIFO_RST | UART_TXFIFO_RST);

    ESP32S3_REG(reg_of(port, UART_CONF1_OFF)) = 0;
    ESP32S3_REG(reg_of(port, UART_INT_CLR_OFF)) = 0xFFFFFFFFu;

    latch_config(port);

    // Pins last: until now the port would have driven rubbish onto them.
    if (tx_pin != UART_PIN_NONE) {
        gpio_route_out(tx_pin, UART_TX_SIG(port));
    }
    if (rx_pin != UART_PIN_NONE) {
        // Pulled up, so an unplugged pin reads as the idle line a UART
        // expects rather than as a permanent stream of break conditions.
        gpio_route_in(rx_pin, UART_RX_SIG(port), GPIO_PULLUP);
    }
}

void uart_write_byte(uint32_t port, uint8_t byte)
{
    if (port >= UART_PORTS) {
        return;
    }

    while (((ESP32S3_REG(reg_of(port, UART_STATUS_OFF)) >> UART_TXFIFO_CNT_S)
            & UART_FIFO_CNT_M) >= UART_FIFO_DEPTH) { }

    // A 32-bit store: a narrower one would become a read-modify-write, and
    // reading this address pops a byte off the *receive* FIFO.
    ESP32S3_REG(reg_of(port, UART_FIFO_OFF)) = byte;
}

void uart_write(uint32_t port, const void *data, uint32_t len)
{
    const uint8_t *bytes = data;

    for (uint32_t i = 0; i < len; i++) {
        uart_write_byte(port, bytes[i]);
    }
}

void uart_print(uint32_t port, const char *text)
{
    while (*text) {
        uart_write_byte(port, (uint8_t)*text++);
    }
}

uint32_t uart_rx_available(uint32_t port)
{
    if (port >= UART_PORTS) {
        return 0;
    }
    return (ESP32S3_REG(reg_of(port, UART_STATUS_OFF)) >> UART_RXFIFO_CNT_S)
           & UART_FIFO_CNT_M;
}

int uart_read_byte(uint32_t port)
{
    if (uart_rx_available(port) == 0) {
        return -1;
    }
    return (int)(ESP32S3_REG(reg_of(port, UART_FIFO_OFF)) & 0xFFu);
}

uint32_t uart_read(uint32_t port, void *buf, uint32_t max_len)
{
    uint8_t *bytes = buf;
    uint32_t count = uart_rx_available(port);

    if (count > max_len) {
        count = max_len;
    }
    for (uint32_t i = 0; i < count; i++) {
        bytes[i] = (uint8_t)(ESP32S3_REG(reg_of(port, UART_FIFO_OFF)) & 0xFFu);
    }
    return count;
}

void uart_drain(uint32_t port)
{
    if (port >= UART_PORTS) {
        return;
    }

    // An empty FIFO is not the same as an idle line: the last byte is still
    // being shifted out a bit at a time. The transmit state machine reads 0
    // only once that has finished.
    while (((ESP32S3_REG(reg_of(port, UART_STATUS_OFF)) >> UART_TXFIFO_CNT_S)
            & UART_FIFO_CNT_M) != 0) { }

    while (((ESP32S3_REG(reg_of(port, UART_FSM_STATUS_OFF)) >> UART_ST_UTX_OUT_S)
            & UART_ST_UTX_OUT_M) != 0) { }
}
