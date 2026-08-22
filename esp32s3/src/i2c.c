/*
 * I2C master: build a command list, let the controller run it, read the
 * outcome out of the interrupt-status register.
 */

#include "i2c.h"
#include "clock.h"
#include "gpio.h"
#include "regs.h"

#define I2C_PORTS   2

// A device may hold the clock low to buy itself thinking time, so there is no
// upper bound on a transaction from the bus's side. The hardware's own
// timeout catches a device that stretches too far; this catches a controller
// that never reports anything at all.
#define I2C_SPIN_LIMIT  2000000u

// Digital glitch filter width, in source-clock ticks. Matches ESP-IDF's
// default: enough to swallow ringing on a long bus, short enough not to eat a
// real edge at 400 kHz.
#define I2C_FILTER_TICKS  7u

// The speeds the timing arithmetic below stays inside its register fields
// for. The bus standard tops out at 1 MHz (Fast-mode Plus) anyway, and the
// low end is where the pre-divider runs out of range.
#define I2C_HZ_MIN      1000u
#define I2C_HZ_MAX      1000000u

static const uint32_t i2c_base[I2C_PORTS] = { I2C0_BASE, I2C1_BASE };

static uint32_t reg_of(uint32_t port, uint32_t offset)
{
    return i2c_base[port < I2C_PORTS ? port : 0] + offset;
}

// Writes one step of the transaction program into command slot n.
static void set_cmd(uint32_t port, uint32_t slot, uint32_t op, uint32_t bytes, uint32_t flags)
{
    ESP32S3_REG(reg_of(port, I2C_COMD_OFF(slot))) =
        (op << I2C_CMD_OP_CODE_S) | (bytes << I2C_CMD_BYTE_NUM_S) | flags;
}

// SCL is a square wave the controller builds out of counted source-clock
// ticks, and the other seven timings are all fractions of its half period.
// The proportions below are ESP-IDF's, and the hardware assumes a specific
// ordering among three of them: the wait-high window has to end before SDA is
// sampled, which in turn has to happen before the high phase does.
static void set_bus_speed(uint32_t port, uint32_t hz)
{
    const uint32_t sclk = ESP32S3_XTAL_MHZ * 1000000u;

    // A pre-divider only becomes necessary below about 38 kHz, where a half
    // period stops fitting in the 9-bit period registers.
    uint32_t pre_div = sclk / (hz * 1024u) + 1u;
    uint32_t tick    = sclk / pre_div;
    uint32_t half    = tick / hz / 2u;

    // Below 80 kHz a wide wait-high window pushes the real frequency up
    // rather than down, so the split changes there.
    uint32_t wait_high = (hz >= 80000u) ? (half / 2u - 2u) : (half / 4u);
    uint32_t high      = half - wait_high;

    uint32_t clk = ESP32S3_REG(reg_of(port, I2C_CLK_CONF_OFF));
    clk &= ~(I2C_SCLK_DIV_NUM_M << I2C_SCLK_DIV_NUM_S);
    clk |= (pre_div - 1u) << I2C_SCLK_DIV_NUM_S;
    clk &= ~I2C_SCLK_SEL_RC_FAST;                   // source = the crystal
    clk |= I2C_SCLK_ACTIVE;
    ESP32S3_REG(reg_of(port, I2C_CLK_CONF_OFF)) = clk;

    ESP32S3_REG(reg_of(port, I2C_SCL_LOW_PERIOD_OFF))   = half - 1u;
    ESP32S3_REG(reg_of(port, I2C_SCL_HIGH_PERIOD_OFF))  =
        high | (wait_high << I2C_SCL_WAIT_HIGH_PERIOD_S);
    ESP32S3_REG(reg_of(port, I2C_SDA_HOLD_OFF))         = half / 4u - 1u;
    ESP32S3_REG(reg_of(port, I2C_SDA_SAMPLE_OFF))       = half / 2u - 1u;
    ESP32S3_REG(reg_of(port, I2C_SCL_RSTART_SETUP_OFF)) = half - 1u;
    ESP32S3_REG(reg_of(port, I2C_SCL_STOP_SETUP_OFF))   = half - 1u;
    ESP32S3_REG(reg_of(port, I2C_SCL_START_HOLD_OFF))   = half - 1u;
    ESP32S3_REG(reg_of(port, I2C_SCL_STOP_HOLD_OFF))    = half - 1u;

    // The timeout is expressed as a power of two of source-clock ticks. Ten
    // bus cycles is long enough for a slow sensor, short enough that a device
    // holding the line down does not wedge the program.
    uint32_t tout = (32u - (uint32_t)__builtin_clz(5u * half)) + 2u;
    ESP32S3_REG(reg_of(port, I2C_TO_OFF)) = tout | I2C_TIME_OUT_EN;
}

void i2c_init(uint32_t port, uint32_t sda_pin, uint32_t scl_pin, uint32_t hz)
{
    if (port >= I2C_PORTS) {
        return;
    }
    if (hz < I2C_HZ_MIN) {
        hz = I2C_HZ_MIN;
    } else if (hz > I2C_HZ_MAX) {
        hz = I2C_HZ_MAX;
    }

    periph_enable(port == 0 ? SYSTEM_PERIP_CLK_EN0_REG : SYSTEM_PERIP_CLK_EN1_REG,
                  port == 0 ? SYSTEM_PERIP_RST_EN0_REG : SYSTEM_PERIP_RST_EN1_REG,
                  port == 0 ? SYSTEM_I2C0_CLK_EN : SYSTEM_I2C1_CLK_EN);

    // Master, MSB first, no multi-master arbitration, and both lines driven
    // open drain - which is what SDA_FORCE_OUT and SCL_FORCE_OUT select,
    // despite reading like the opposite.
    ESP32S3_REG(reg_of(port, I2C_CTR_OFF)) =
        I2C_MS_MODE | I2C_SDA_FORCE_OUT | I2C_SCL_FORCE_OUT | I2C_CLK_EN;

    set_bus_speed(port, hz);

    ESP32S3_REG(reg_of(port, I2C_FILTER_CFG_OFF)) =
        I2C_SCL_FILTER_EN | I2C_SDA_FILTER_EN
        | I2C_FILTER_TICKS | (I2C_FILTER_TICKS << I2C_SDA_FILTER_THRES_S);

    ESP32S3_REG(reg_of(port, I2C_FIFO_CONF_OFF)) = I2C_FIFO_PRT_EN;   // FIFO mode
    ESP32S3_REG(reg_of(port, I2C_INT_CLR_OFF))   = I2C_INT_ALL;
    ESP32S3_REG(reg_of(port, I2C_CTR_OFF))      |= I2C_CONF_UPGATE;

    // Both lines carry the signal in both directions, so each pad is wired to
    // the matrix twice over - see gpio_route_open_drain().
    gpio_route_open_drain(sda_pin, I2C_SDA_SIG(port), GPIO_PULLUP);
    gpio_route_open_drain(scl_pin, I2C_SCL_SIG(port), GPIO_PULLUP);
}

// Hands the command list and the FIFO to the controller, then waits for it to
// finish and works out what happened.
static i2c_status_t run(uint32_t port)
{
    ESP32S3_REG(reg_of(port, I2C_INT_CLR_OFF)) = I2C_INT_ALL;
    ESP32S3_REG(reg_of(port, I2C_CTR_OFF))    |= I2C_CONF_UPGATE;
    ESP32S3_REG(reg_of(port, I2C_CTR_OFF))    |= I2C_TRANS_START;

    const uint32_t done = I2C_INT_TRANS_COMPLETE | I2C_INT_NACK
                        | I2C_INT_TIME_OUT | I2C_INT_ARBITRATION_LOST;

    uint32_t status = 0;
    for (uint32_t spin = 0; spin < I2C_SPIN_LIMIT; spin++) {
        status = ESP32S3_REG(reg_of(port, I2C_INT_RAW_OFF));
        if (status & done) {
            break;
        }
    }

    ESP32S3_REG(reg_of(port, I2C_INT_CLR_OFF)) = I2C_INT_ALL;

    // A NACK can be reported alongside TRANS_COMPLETE, so check it first.
    if (status & I2C_INT_NACK) {
        return I2C_ERR_NACK;
    }
    if (status & I2C_INT_ARBITRATION_LOST) {
        return I2C_ERR_ARBITRATION;
    }
    if (status & I2C_INT_TRANS_COMPLETE) {
        return I2C_OK;
    }

    // Timed out, or never reported anything. Either way the state machine is
    // mid-transaction and has to be put back to a known state by hand.
    ESP32S3_REG(reg_of(port, I2C_CTR_OFF)) |= I2C_FSM_RST;
    ESP32S3_REG(reg_of(port, I2C_CTR_OFF)) |= I2C_CONF_UPGATE;
    return I2C_ERR_TIMEOUT;
}

// Empties both FIFOs so a failed transaction leaves nothing behind for the
// next one to send.
static void reset_fifos(uint32_t port)
{
    uint32_t conf = ESP32S3_REG(reg_of(port, I2C_FIFO_CONF_OFF));
    ESP32S3_REG(reg_of(port, I2C_FIFO_CONF_OFF)) = conf | I2C_TX_FIFO_RST | I2C_RX_FIFO_RST;
    ESP32S3_REG(reg_of(port, I2C_FIFO_CONF_OFF)) = conf;
}

static void push(uint32_t port, uint8_t byte)
{
    ESP32S3_REG(reg_of(port, I2C_DATA_OFF)) = byte;
}

static uint8_t pop(uint32_t port)
{
    return (uint8_t)(ESP32S3_REG(reg_of(port, I2C_DATA_OFF)) & 0xFFu);
}

i2c_status_t i2c_write(uint32_t port, uint8_t addr, const void *data, uint32_t len)
{
    const uint8_t *bytes = data;

    if (port >= I2C_PORTS) {
        return I2C_ERR_ARG;
    }
    if (len + 1 > I2C_MAX_TRANSFER) {   // the address byte shares the FIFO
        return I2C_ERR_TOO_LONG;
    }

    reset_fifos(port);

    push(port, (uint8_t)(addr << 1));               // bit 0 clear: writing
    for (uint32_t i = 0; i < len; i++) {
        push(port, bytes[i]);
    }

    // ACK_CHECK_EN makes the controller abandon the transaction if a device
    // fails to acknowledge, rather than talking to nobody for len bytes.
    set_cmd(port, 0, I2C_CMD_RESTART, 0, 0);
    set_cmd(port, 1, I2C_CMD_WRITE, len + 1, I2C_CMD_ACK_CHECK_EN);
    set_cmd(port, 2, I2C_CMD_STOP, 0, 0);

    return run(port);
}

i2c_status_t i2c_read(uint32_t port, uint8_t addr, void *buf, uint32_t len)
{
    uint8_t *bytes = buf;

    if (port >= I2C_PORTS) {
        return I2C_ERR_ARG;
    }
    if (len == 0 || len > I2C_MAX_TRANSFER) {
        return I2C_ERR_TOO_LONG;
    }

    reset_fifos(port);
    push(port, (uint8_t)((addr << 1) | 1u));        // bit 0 set: reading

    uint32_t slot = 0;
    set_cmd(port, slot++, I2C_CMD_RESTART, 0, 0);
    set_cmd(port, slot++, I2C_CMD_WRITE, 1, I2C_CMD_ACK_CHECK_EN);

    // Every byte but the last is acknowledged to ask for another. The last
    // one is not, which is the only way a master says "that will do".
    if (len > 1) {
        set_cmd(port, slot++, I2C_CMD_READ, len - 1, 0);
    }
    set_cmd(port, slot++, I2C_CMD_READ, 1, I2C_CMD_ACK_VALUE);
    set_cmd(port, slot++, I2C_CMD_STOP, 0, 0);

    i2c_status_t status = run(port);
    if (status != I2C_OK) {
        return status;
    }

    for (uint32_t i = 0; i < len; i++) {
        bytes[i] = pop(port);
    }
    return I2C_OK;
}

i2c_status_t i2c_write_read(uint32_t port, uint8_t addr,
                            const void *tx, uint32_t tx_len,
                            void *rx, uint32_t rx_len)
{
    const uint8_t *out = tx;
    uint8_t *in = rx;

    if (port >= I2C_PORTS) {
        return I2C_ERR_ARG;
    }
    // Both address bytes and the outgoing data share the one transmit FIFO.
    if (tx_len == 0 || tx_len + 2 > I2C_MAX_TRANSFER
        || rx_len == 0 || rx_len > I2C_MAX_TRANSFER) {
        return I2C_ERR_TOO_LONG;
    }

    reset_fifos(port);

    push(port, (uint8_t)(addr << 1));
    for (uint32_t i = 0; i < tx_len; i++) {
        push(port, out[i]);
    }
    push(port, (uint8_t)((addr << 1) | 1u));

    // Seven slots at most, and the hardware has eight.
    uint32_t slot = 0;
    set_cmd(port, slot++, I2C_CMD_RESTART, 0, 0);
    set_cmd(port, slot++, I2C_CMD_WRITE, tx_len + 1, I2C_CMD_ACK_CHECK_EN);
    set_cmd(port, slot++, I2C_CMD_RESTART, 0, 0);   // turn the bus around
    set_cmd(port, slot++, I2C_CMD_WRITE, 1, I2C_CMD_ACK_CHECK_EN);
    if (rx_len > 1) {
        set_cmd(port, slot++, I2C_CMD_READ, rx_len - 1, 0);
    }
    set_cmd(port, slot++, I2C_CMD_READ, 1, I2C_CMD_ACK_VALUE);
    set_cmd(port, slot++, I2C_CMD_STOP, 0, 0);

    i2c_status_t status = run(port);
    if (status != I2C_OK) {
        return status;
    }

    for (uint32_t i = 0; i < rx_len; i++) {
        in[i] = pop(port);
    }
    return I2C_OK;
}

i2c_status_t i2c_probe(uint32_t port, uint8_t addr)
{
    return i2c_write(port, addr, 0, 0);
}
