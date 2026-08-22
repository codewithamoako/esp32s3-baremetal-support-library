/*
 * ADC1 one-shot conversions through the RTC controller.
 *
 * Three blocks of registers: SENS is the SAR controller, RTC IO is the pad -
 * an analog input is not a GPIO and has to leave the digital mux entirely -
 * and SYSTEM gates the ADC's APB clock.
 *
 * Addresses and field positions are from the ESP32-S3 TRM, matched against
 * ESP-IDF's sens_reg.h, rtc_io_reg.h and adc_ll.h. No ESP-IDF headers here.
 */

#include "adc.h"
#include "clock.h"
#include "regs.h"

// SENS - the SAR ADC controller
#define SENS_BASE                       0x60008800u
#define SENS_SAR_READER1_CTRL_REG       (SENS_BASE + 0x000)
#define SENS_SAR_MEAS1_CTRL2_REG        (SENS_BASE + 0x00C)
#define SENS_SAR_MEAS1_MUX_REG          (SENS_BASE + 0x010)
#define SENS_SAR_ATTEN1_REG             (SENS_BASE + 0x014)
#define SENS_SAR_POWER_XPD_SAR_REG      (SENS_BASE + 0x03C)
#define SENS_SAR_SLAVE_ADDR1_REG        (SENS_BASE + 0x040)
#define SENS_SAR_PERI_CLK_GATE_CONF_REG (SENS_BASE + 0x104)

#define SENS_SAR1_CLK_DIV_S             0        // 8 bits: the SAR's own clock
#define SENS_SAR1_CLK_DIV_M             0xFFu
#define SENS_SAR1_DATA_INV              (1u << 28)
#define SENS_MEAS1_DATA_SAR_M           0xFFFFu  // 12 bits are used of it
#define SENS_MEAS1_DONE_SAR             (1u << 16)
#define SENS_MEAS1_START_SAR            (1u << 17)   // 0 -> 1 starts a conversion
#define SENS_MEAS1_START_FORCE          (1u << 18)   // 1 = software, not the FSM
#define SENS_SAR1_EN_PAD_S              19       // 12 bits: one per channel
#define SENS_SAR1_EN_PAD_M              0xFFFu
#define SENS_SAR1_EN_PAD_FORCE          (1u << 31)   // 1 = software picks the channel
#define SENS_SAR1_DIG_FORCE             (1u << 31)   // 1 = the digital scanner owns ADC1
#define SENS_MEAS_STATUS_S              22       // busy while non-zero
#define SENS_MEAS_STATUS_M              0xFFu
#define SENS_FORCE_XPD_SAR_S            29
#define SENS_FORCE_XPD_SAR_M            0x3u
#define SENS_FORCE_XPD_SAR_FSM          0u       // powered only while measuring
#define SENS_FORCE_XPD_SAR_ON           3u       // powered always
#define SENS_SARADC_CLK_EN              (1u << 30)

// RTC IO - the pad side. For GPIO0..GPIO21 the RTC pad number is the GPIO
// number, so the pin indexes these registers directly.
#define RTCIO_BASE                      0x60008400u
#define RTC_GPIO_ENABLE_W1TC_REG        (RTCIO_BASE + 0x014)
#define RTC_GPIO_ENABLE_S               10
#define RTC_IO_PAD_REG(n)               (RTCIO_BASE + 0x084 + 4u * (n))
#define RTC_IO_PAD_FUN_IE               (1u << 13)   // digital input buffer
#define RTC_IO_PAD_SLP_SEL              (1u << 16)
#define RTC_IO_PAD_FUN_SEL_S            17
#define RTC_IO_PAD_FUN_SEL_M            0x3u
#define RTC_IO_PAD_MUX_SEL              (1u << 19)   // 1 = RTC/analog owns the pad
#define RTC_IO_PAD_XPD                  (1u << 20)   // touch sensor on this pad
#define RTC_IO_PAD_TIE_OPT              (1u << 21)
#define RTC_IO_PAD_START                (1u << 22)
#define RTC_IO_PAD_RUE                  (1u << 27)   // pull-up
#define RTC_IO_PAD_RDE                  (1u << 28)   // pull-down

#define SYSTEM_APB_SARADC_CLK_EN        (1u << 28)   // and the same bit in RST_EN0

// ADC1 covers GPIO1..GPIO10, in order, as channels 0..9.
#define ADC1_FIRST_PIN                  1u
#define ADC1_LAST_PIN                   10u

int adc_channel_for_pin(uint32_t pin)
{
    if (pin < ADC1_FIRST_PIN || pin > ADC1_LAST_PIN) {
        return -1;
    }
    return (int)(pin - ADC1_FIRST_PIN);
}

// No input buffer, no pulls, no touch sensor, nobody driving it, and the mux
// pointed at the analog side. A pull-up here would bias what is measured.
static void pad_to_analog(uint32_t pin)
{
    uint32_t cfg = ESP32S3_REG(RTC_IO_PAD_REG(pin));

    cfg &= ~(RTC_IO_PAD_FUN_IE | RTC_IO_PAD_SLP_SEL | RTC_IO_PAD_RUE | RTC_IO_PAD_RDE
             | RTC_IO_PAD_XPD | RTC_IO_PAD_TIE_OPT | RTC_IO_PAD_START
             | (RTC_IO_PAD_FUN_SEL_M << RTC_IO_PAD_FUN_SEL_S));
    cfg |= RTC_IO_PAD_MUX_SEL;
    ESP32S3_REG(RTC_IO_PAD_REG(pin)) = cfg;

    // Neither output driver may fight the signal.
    ESP32S3_REG(RTC_GPIO_ENABLE_W1TC_REG) = 1u << (RTC_GPIO_ENABLE_S + pin);
    ESP32S3_REG(GPIO_ENABLE_W1TC_REG)     = 1u << pin;
}

int adc_init(uint32_t pin, adc_atten_t atten)
{
    int channel = adc_channel_for_pin(pin);

    if (channel < 0) {
        return 0;
    }

    periph_enable(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_PERIP_RST_EN0_REG, SYSTEM_APB_SARADC_CLK_EN);

    // Force the analog block on: left to its state machine it wakes per
    // conversion, which costs longer than the conversion.
    ESP32S3_REG(SENS_SAR_PERI_CLK_GATE_CONF_REG) |= SENS_SARADC_CLK_EN;

    uint32_t power = ESP32S3_REG(SENS_SAR_POWER_XPD_SAR_REG);
    power = (power & ~(SENS_FORCE_XPD_SAR_M << SENS_FORCE_XPD_SAR_S))
          | (SENS_FORCE_XPD_SAR_ON << SENS_FORCE_XPD_SAR_S);
    ESP32S3_REG(SENS_SAR_POWER_XPD_SAR_REG) = power;

    // Fastest SAR clock, and the result the right way up.
    uint32_t reader = ESP32S3_REG(SENS_SAR_READER1_CTRL_REG);
    reader = (reader & ~(SENS_SAR1_CLK_DIV_M << SENS_SAR1_CLK_DIV_S))
           | (1u << SENS_SAR1_CLK_DIV_S);
    reader &= ~SENS_SAR1_DATA_INV;
    ESP32S3_REG(SENS_SAR_READER1_CTRL_REG) = reader;

    // Claim ADC1 for software, not the digital scanner or the ULP.
    ESP32S3_REG(SENS_SAR_MEAS1_MUX_REG)  &= ~SENS_SAR1_DIG_FORCE;
    ESP32S3_REG(SENS_SAR_MEAS1_CTRL2_REG) |= SENS_MEAS1_START_FORCE | SENS_SAR1_EN_PAD_FORCE;

    // Attenuation is per channel, two bits each.
    uint32_t attens = ESP32S3_REG(SENS_SAR_ATTEN1_REG);
    attens = (attens & ~(0x3u << (channel * 2)))
           | (((uint32_t)atten & 0x3u) << (channel * 2));
    ESP32S3_REG(SENS_SAR_ATTEN1_REG) = attens;

    pad_to_analog(pin);
    return 1;
}

uint16_t adc_read(uint32_t pin)
{
    int channel = adc_channel_for_pin(pin);

    if (channel < 0) {
        return 0;
    }

    // Shared between channels: wait for anything still in flight.
    while ((ESP32S3_REG(SENS_SAR_SLAVE_ADDR1_REG) >> SENS_MEAS_STATUS_S) & SENS_MEAS_STATUS_M) {
    }

    // Start is edge-triggered, so the channel goes in with it still clear.
    uint32_t ctrl = ESP32S3_REG(SENS_SAR_MEAS1_CTRL2_REG);
    ctrl &= ~((SENS_SAR1_EN_PAD_M << SENS_SAR1_EN_PAD_S) | SENS_MEAS1_START_SAR);
    ctrl |= 1u << (SENS_SAR1_EN_PAD_S + channel);
    ESP32S3_REG(SENS_SAR_MEAS1_CTRL2_REG) = ctrl;
    ESP32S3_REG(SENS_SAR_MEAS1_CTRL2_REG) = ctrl | SENS_MEAS1_START_SAR;

    while (!(ESP32S3_REG(SENS_SAR_MEAS1_CTRL2_REG) & SENS_MEAS1_DONE_SAR)) {
    }

    return (uint16_t)(ESP32S3_REG(SENS_SAR_MEAS1_CTRL2_REG) & ADC_MAX);
}
