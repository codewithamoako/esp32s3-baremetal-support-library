/*
 * CPU clock: reads the current speed from the clock tree and switches to
 * 160 MHz off the PLL.
 */

#include "clock.h"
#include "regs.h"

static uint32_t cached_cpu_mhz;     // 0 until someone asks

uint32_t read_cpu_mhz(void)
{
    uint32_t sysclk = ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG);

    switch ((sysclk >> SYSTEM_SOC_CLK_SEL_S) & SYSTEM_SOC_CLK_SEL_M) {
    case 0:                                 // straight off the crystal
        return ESP32S3_XTAL_MHZ / ((sysclk & SYSTEM_PRE_DIV_CNT_M) + 1);
    case 1:                                 // PLL, divided down
        switch (ESP32S3_REG(SYSTEM_CPU_PER_CONF_REG) & SYSTEM_CPUPERIOD_SEL_M) {
        case 0:  return 80;
        case 1:  return 160;
        default: return 240;
        }
    default:                                // internal RC oscillator
        return 20;
    }
}

uint32_t cpu_mhz(void)
{
    if (cached_cpu_mhz == 0) {
        cached_cpu_mhz = read_cpu_mhz();
    }
    return cached_cpu_mhz;
}

void set_cpu_160mhz(void)
{
    uint32_t period = ESP32S3_REG(SYSTEM_CPU_PER_CONF_REG);
    period = (period & ~SYSTEM_CPUPERIOD_SEL_M) | 1u;   // CPUPERIOD_SEL = 160 MHz
    ESP32S3_REG(SYSTEM_CPU_PER_CONF_REG) = period;

    uint32_t sysclk = ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG);
    sysclk &= ~SYSTEM_PRE_DIV_CNT_M;                    // divide by 1
    ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG) = sysclk;

    sysclk = (sysclk & ~(SYSTEM_SOC_CLK_SEL_M << SYSTEM_SOC_CLK_SEL_S))
           | (1u << SYSTEM_SOC_CLK_SEL_S);              // source = PLL
    ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG) = sysclk;

    cached_cpu_mhz = read_cpu_mhz();
}

void periph_enable(uint32_t clk_en_reg, uint32_t rst_en_reg, uint32_t mask)
{
    ESP32S3_REG(clk_en_reg) |= mask;

    // Held in reset, then let go: whatever the ROM left in the peripheral's
    // registers is cleared before its driver writes the first one.
    ESP32S3_REG(rst_en_reg) |= mask;
    ESP32S3_REG(rst_en_reg) &= ~mask;
}
