/*
 * ESP32-S3 register map - just the peripherals this project touches.
 *
 * Addresses come from the ESP32-S3 TRM, cross-checked against the soc header
 * files in ESP-IDF. Nothing here includes an ESP-IDF header.
 */

#ifndef ESP32S3_REGS_H
#define ESP32S3_REGS_H

#include <stdint.h>

// Every register access in this project goes through here.
#define ESP32S3_REG(addr)   (*(volatile uint32_t *)(uintptr_t)(addr))

// Every ESP32-S3 module is built around a 40 MHz crystal.
#define ESP32S3_XTAL_MHZ    40

// GPIO and IO MUX
#define GPIO_BASE                       0x60004000u
#define GPIO_OUT_W1TS_REG               (GPIO_BASE + 0x008)
#define GPIO_OUT_W1TC_REG               (GPIO_BASE + 0x00C)
#define GPIO_ENABLE_W1TS_REG            (GPIO_BASE + 0x024)
#define GPIO_ENABLE_W1TC_REG            (GPIO_BASE + 0x028)
#define GPIO_FUNC_OUT_SEL_CFG_REG(n)    (GPIO_BASE + 0x554 + 4u * (n))

#define IO_MUX_BASE                     0x60009000u
#define IO_MUX_GPIO_REG(n)              (IO_MUX_BASE + 0x004 + 4u * (n))
#define IO_MUX_MCU_SEL_S                12
#define IO_MUX_FUN_DRV_S                10
#define IO_MUX_FUNC_GPIO                1     // IO MUX function 1 == GPIO
#define GPIO_SIG_OUT_IDX                256   // pad driven from GPIO_OUT_REG

// Watchdogs - the ROM arms these before jumping into our image
#define RTC_CNTL_BASE                   0x60008000u
#define RTC_CNTL_WDTCONFIG0_REG         (RTC_CNTL_BASE + 0x098)
#define RTC_CNTL_WDTWPROTECT_REG        (RTC_CNTL_BASE + 0x0B0)
#define RTC_CNTL_SWD_CONF_REG           (RTC_CNTL_BASE + 0x0B4)
#define RTC_CNTL_SWD_WPROTECT_REG       (RTC_CNTL_BASE + 0x0B8)
#define RTC_CNTL_SWD_AUTO_FEED_EN       (1u << 31)
#define RTC_CNTL_WDT_WKEY_VALUE         0x50D83AA1u
#define RTC_CNTL_SWD_WKEY_VALUE         0x8F1D312Au

#define TIMG0_BASE                      0x6001F000u
#define TIMG0_WDTCONFIG0_REG            (TIMG0_BASE + 0x048)
#define TIMG0_WDTWPROTECT_REG           (TIMG0_BASE + 0x064)
#define TIMG_WDT_WKEY_VALUE             0x50D83AA1u

// Clock tree
#define SYSTEM_BASE                     0x600C0000u
#define SYSTEM_CPU_PER_CONF_REG         (SYSTEM_BASE + 0x010)
#define SYSTEM_SYSCLK_CONF_REG          (SYSTEM_BASE + 0x060)
#define SYSTEM_CPUPERIOD_SEL_M          0x3u
#define SYSTEM_PRE_DIV_CNT_M            0x3FFu
#define SYSTEM_SOC_CLK_SEL_S            10
#define SYSTEM_SOC_CLK_SEL_M            0x3u

// USB-Serial-JTAG - the single USB-C port, already enumerated by ROM
#define USB_SERIAL_JTAG_BASE            0x60038000u
#define USB_SERIAL_JTAG_EP1_REG         (USB_SERIAL_JTAG_BASE + 0x000)
#define USB_SERIAL_JTAG_EP1_CONF_REG    (USB_SERIAL_JTAG_BASE + 0x004)
#define USB_SERIAL_JTAG_WR_DONE         (1u << 0)
#define USB_SERIAL_JTAG_IN_EP_DATA_FREE (1u << 1)

// Peripheral clock gating and reset. Each peripheral below powers up with its
// clock gated off, so ungating is the first thing every driver does.
#define SYSTEM_PERIP_CLK_EN0_REG        (SYSTEM_BASE + 0x018)
#define SYSTEM_PERIP_CLK_EN1_REG        (SYSTEM_BASE + 0x01C)
#define SYSTEM_PERIP_RST_EN0_REG        (SYSTEM_BASE + 0x020)
#define SYSTEM_PERIP_RST_EN1_REG        (SYSTEM_BASE + 0x024)
#define SYSTEM_UART0_CLK_EN             (1u << 2)     // in the ...EN0 pair
#define SYSTEM_UART1_CLK_EN             (1u << 5)
#define SYSTEM_UART2_CLK_EN             (1u << 9)     // in the ...EN1 pair
#define SYSTEM_SPI2_CLK_EN              (1u << 6)
#define SYSTEM_I2C0_CLK_EN              (1u << 7)
#define SYSTEM_I2C1_CLK_EN              (1u << 18)
#define SYSTEM_LEDC_CLK_EN              (1u << 11)

// GPIO matrix and pad control beyond the plain output pin
#define GPIO_OUT_REG                    (GPIO_BASE + 0x004)
#define GPIO_OUT1_W1TS_REG              (GPIO_BASE + 0x014)   // pins 32..48
#define GPIO_OUT1_W1TC_REG              (GPIO_BASE + 0x018)
#define GPIO_ENABLE_REG                 (GPIO_BASE + 0x020)
#define GPIO_ENABLE1_W1TS_REG           (GPIO_BASE + 0x030)   // pins 32..48
#define GPIO_ENABLE1_W1TC_REG           (GPIO_BASE + 0x034)
#define GPIO_IN_REG                     (GPIO_BASE + 0x03C)
#define GPIO_IN1_REG                    (GPIO_BASE + 0x040)
#define GPIO_PIN_REG(n)                 (GPIO_BASE + 0x074 + 4u * (n))
#define GPIO_PIN_PAD_DRIVER             (1u << 2)     // 1 = open drain
#define GPIO_FUNC_IN_SEL_CFG_REG(sig)   (GPIO_BASE + 0x154 + 4u * (sig))
#define GPIO_FUNC_IN_SEL_M              0x3Fu
#define GPIO_SIG_IN_SEL                 (1u << 7)     // 1 = take it from the matrix
// Left clear throughout: with it clear the peripheral's own output-enable
// gates the pad, which is what lets I2C let go of a line it is not pulling.
#define GPIO_FUNC_OEN_SEL               (1u << 11)    // 1 = GPIO_ENABLE owns the OE

#define IO_MUX_FUN_PD_S                 7
#define IO_MUX_FUN_PU_S                 8
#define IO_MUX_FUN_IE_S                 9

// GPIO matrix signal numbers, from the ESP32-S3 peripheral signal list. A
// signal has one number used for both directions.
#define UART_TX_SIG(n)                  ((n) == 0 ? 12u : (n) == 1 ? 15u : 18u)
#define UART_RX_SIG(n)                  UART_TX_SIG(n)
#define I2C_SCL_SIG(n)                  ((n) == 0 ? 89u : 91u)
#define I2C_SDA_SIG(n)                  ((n) == 0 ? 90u : 92u)
#define SPI2_CLK_SIG                    101u
#define SPI2_MISO_SIG                   102u          // "Q" in Espressif's naming
#define SPI2_MOSI_SIG                   103u          // "D" in Espressif's naming
#define SPI2_CS0_SIG                    110u
#define LEDC_SIG(ch)                    (73u + (ch))

// UART0/1/2. The three bases are not evenly spaced, so drivers index a table.
#define UART0_BASE                      0x60000000u
#define UART1_BASE                      0x60010000u
#define UART2_BASE                      0x6002E000u
#define UART_FIFO_OFF                   0x000    // read pops rx, write pushes tx
#define UART_INT_CLR_OFF                0x010
#define UART_CLKDIV_OFF                 0x014
#define UART_STATUS_OFF                 0x01C
#define UART_CONF0_OFF                  0x020
#define UART_CONF1_OFF                  0x024
#define UART_FSM_STATUS_OFF             0x06C
#define UART_CLK_CONF_OFF               0x078
#define UART_ID_OFF                     0x080
#define UART_FIFO_DEPTH                 128
#define UART_CLKDIV_M                   0xFFFu   // whole part, 12 bits
#define UART_CLKDIV_FRAG_S              20       // sixteenths, 4 bits
#define UART_RXFIFO_CNT_S               0
#define UART_TXFIFO_CNT_S               16
#define UART_FIFO_CNT_M                 0x3FFu
#define UART_ST_UTX_OUT_S               4        // in FSM_STATUS
#define UART_ST_UTX_OUT_M               0xFu
#define UART_BIT_NUM_S                  2        // 0=5, 1=6, 2=7, 3=8 data bits
#define UART_STOP_BIT_NUM_S             4        // 1=1, 2=1.5, 3=2 stop bits
#define UART_RXFIFO_RST                 (1u << 17)
#define UART_TXFIFO_RST                 (1u << 18)
// Gates the clock the two FIFOs are built on, and is the one bit in CONF0
// that powers up set - so a write that means "8N1 and nothing else" has to
// name it explicitly rather than leave it zero.
#define UART_MEM_CLK_EN                 (1u << 28)
#define UART_SCLK_DIV_NUM_S             12       // pre-divider, minus one
#define UART_SCLK_DIV_NUM_M             0xFFu
#define UART_SCLK_SEL_S                 20       // 1=80M, 2=RC_FAST, 3=XTAL
#define UART_SCLK_SEL_XTAL              3u
#define UART_SCLK_EN                    (1u << 22)
#define UART_RST_CORE                   (1u << 23)
#define UART_TX_SCLK_EN                 (1u << 24)
#define UART_RX_SCLK_EN                 (1u << 25)
#define UART_REG_UPDATE                 (1u << 31)    // in UART_ID

// I2C0/1
#define I2C0_BASE                       0x60013000u
#define I2C1_BASE                       0x60027000u
#define I2C_SCL_LOW_PERIOD_OFF          0x000
#define I2C_CTR_OFF                     0x004
#define I2C_SR_OFF                      0x008
#define I2C_TO_OFF                      0x00C
#define I2C_FIFO_CONF_OFF               0x018
#define I2C_DATA_OFF                    0x01C
#define I2C_INT_RAW_OFF                 0x020
#define I2C_INT_CLR_OFF                 0x024
#define I2C_SDA_HOLD_OFF                0x030
#define I2C_SDA_SAMPLE_OFF              0x034
#define I2C_SCL_HIGH_PERIOD_OFF         0x038
#define I2C_SCL_START_HOLD_OFF          0x040
#define I2C_SCL_RSTART_SETUP_OFF        0x044
#define I2C_SCL_STOP_HOLD_OFF           0x048
#define I2C_SCL_STOP_SETUP_OFF          0x04C
#define I2C_FILTER_CFG_OFF              0x050
#define I2C_CLK_CONF_OFF                0x054
#define I2C_COMD_OFF(n)                 (0x058 + 4u * (n))
#define I2C_CMD_SLOTS                   8
#define I2C_FIFO_DEPTH                  32
#define I2C_SDA_FORCE_OUT               (1u << 0)   // 1 = open drain, as I2C wants
#define I2C_SCL_FORCE_OUT               (1u << 1)
#define I2C_SAMPLE_SCL_LEVEL            (1u << 2)
#define I2C_MS_MODE                     (1u << 4)   // 1 = master
#define I2C_TRANS_START                 (1u << 5)
#define I2C_CLK_EN                      (1u << 8)   // register clock gate
#define I2C_FSM_RST                     (1u << 10)
#define I2C_CONF_UPGATE                 (1u << 11)  // latch config into the core
#define I2C_SR_BUS_BUSY                 (1u << 4)
#define I2C_RX_FIFO_RST                 (1u << 12)
#define I2C_TX_FIFO_RST                 (1u << 13)
#define I2C_FIFO_PRT_EN                 (1u << 14)
#define I2C_SCL_WAIT_HIGH_PERIOD_S      9
#define I2C_TIME_OUT_EN                 (1u << 5)
#define I2C_SCL_FILTER_EN               (1u << 8)
#define I2C_SDA_FILTER_EN               (1u << 9)
#define I2C_SDA_FILTER_THRES_S          4
#define I2C_SCLK_DIV_NUM_S              0           // minus one
#define I2C_SCLK_DIV_NUM_M              0xFFu
#define I2C_SCLK_SEL_RC_FAST            (1u << 20)  // clear for XTAL
#define I2C_SCLK_ACTIVE                 (1u << 21)
#define I2C_INT_END_DETECT              (1u << 3)
#define I2C_INT_ARBITRATION_LOST        (1u << 5)
#define I2C_INT_TRANS_COMPLETE          (1u << 7)
#define I2C_INT_TIME_OUT                (1u << 8)
#define I2C_INT_NACK                    (1u << 10)
#define I2C_INT_ALL                     0x3FFFFu

// I2C command words.
//
// These opcodes are NOT the ones written in the comment in Espressif's own
// i2c_reg.h. That text is inherited from the original ESP32, where RSTART,
// READ and STOP were 0, 2 and 3. The ESP32-S3 renumbered them; the values
// below match ESP-IDF's esp32s3 i2c_ll.h, which is what actually runs here.
#define I2C_CMD_RESTART                 6u
#define I2C_CMD_WRITE                   1u
#define I2C_CMD_READ                    3u
#define I2C_CMD_STOP                    2u
#define I2C_CMD_END                     4u
#define I2C_CMD_BYTE_NUM_S              0           // bytes moved by this command
#define I2C_CMD_ACK_CHECK_EN            (1u << 8)   // writing: fail on a NACK
#define I2C_CMD_ACK_EXP                 (1u << 9)   // writing: the ACK level wanted
#define I2C_CMD_ACK_VALUE               (1u << 10)  // reading: the ACK level we send
#define I2C_CMD_OP_CODE_S               11

// GP-SPI2, the general-purpose SPI controller
#define SPI2_BASE                       0x60024000u
#define SPI_CMD_OFF                     0x000
#define SPI_CLOCK_OFF                   0x00C
#define SPI_USER_OFF                    0x010
#define SPI_USER1_OFF                   0x014
#define SPI_USER2_OFF                   0x018
#define SPI_MS_DLEN_OFF                 0x01C
#define SPI_MISC_OFF                    0x020
#define SPI_DMA_CONF_OFF                0x030
#define SPI_W_OFF(n)                    (0x098 + 4u * (n))
#define SPI_SLAVE_OFF                   0x0E0
#define SPI_CLK_GATE_OFF                0x0E8
#define SPI_BUF_WORDS                   16          // W0..W15, so 64 bytes a go
#define SPI_UPDATE                      (1u << 23)  // latch config, self-clearing
#define SPI_USR                         (1u << 24)  // start, self-clearing
#define SPI_CLKCNT_L_S                  0
#define SPI_CLKCNT_H_S                  6
#define SPI_CLKCNT_N_S                  12
#define SPI_CLKDIV_PRE_S                18
#define SPI_CLK_EQU_SYSCLK              (1u << 31)
#define SPI_DOUTDIN                     (1u << 0)   // full duplex
#define SPI_CK_OUT_EDGE                 (1u << 9)
#define SPI_USR_MOSI                    (1u << 27)
#define SPI_USR_MISO                    (1u << 28)
#define SPI_USR_DUMMY                   (1u << 29)
#define SPI_USR_ADDR                    (1u << 30)
#define SPI_USR_COMMAND                 (1u << 31)
#define SPI_CS0_DIS                     (1u << 0)
#define SPI_CS1_DIS                     (1u << 1)
#define SPI_CS2_DIS                     (1u << 2)
#define SPI_CK_IDLE_EDGE                (1u << 29)
#define SPI_CS_KEEP_ACTIVE              (1u << 30)
#define SPI_RX_AFIFO_RST                (1u << 29)
#define SPI_BUF_AFIFO_RST               (1u << 30)
#define SPI_SLAVE_MODE                  (1u << 26)
#define SPI_MST_CLK_ACTIVE              (1u << 1)
#define SPI_MST_CLK_SEL                 (1u << 2)   // 1 = 80 MHz PLL, 0 = XTAL
#define SPI_CLK_GATE_EN                 (1u << 0)

// LEDC, the PWM generator. The ESP32-S3 has low-speed channels only.
#define LEDC_BASE                       0x60019000u
#define LEDC_CH_CONF0_OFF(ch)           (0x000 + 0x14u * (ch))
#define LEDC_CH_HPOINT_OFF(ch)          (0x004 + 0x14u * (ch))
#define LEDC_CH_DUTY_OFF(ch)            (0x008 + 0x14u * (ch))
#define LEDC_CH_CONF1_OFF(ch)           (0x00C + 0x14u * (ch))
#define LEDC_TIMER_CONF_OFF(t)          (0x0A0 + 0x08u * (t))
#define LEDC_CONF_OFF                   0x0D0
#define LEDC_CHANNELS                   8
#define LEDC_TIMERS                     4
#define LEDC_TIMER_SEL_S                0
#define LEDC_SIG_OUT_EN                 (1u << 2)
#define LEDC_IDLE_LV                    (1u << 3)
#define LEDC_CH_PARA_UP                 (1u << 4)
#define LEDC_DUTY_RES_S                 0     // duty resolution in bits, 1..14
#define LEDC_CLK_DIV_S                  4     // 18 bits, the low 8 fractional
#define LEDC_TIMER_PAUSE                (1u << 22)
#define LEDC_TIMER_RST                  (1u << 23)
#define LEDC_TIMER_PARA_UP              (1u << 25)
#define LEDC_DUTY_SCALE_S               0     // all four are the fade generator
#define LEDC_DUTY_CYCLE_S               10
#define LEDC_DUTY_NUM_S                 20
#define LEDC_DUTY_INC                   (1u << 30)
#define LEDC_DUTY_START                 (1u << 31)
#define LEDC_DUTY_FRAC_BITS             4     // the duty field is 4 bits wider
#define LEDC_APB_CLK_SEL_XTAL           3u    // 1=APB(80M), 2=RC_FAST, 3=XTAL(40M)
#define LEDC_GLOBAL_CLK_EN              (1u << 31)

#endif // ESP32S3_REGS_H
