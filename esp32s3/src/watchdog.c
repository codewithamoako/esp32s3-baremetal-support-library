/*
 * Disables the watchdogs the ROM arms before jumping into our image.
 */

#include "watchdog.h"
#include "regs.h"

// Each watchdog sits behind a write-protect register: store the magic key to
// unlock it, write the config, then store anything else to lock it again.
void disable_watchdogs(void)
{
    // The super watchdog cannot be disabled, only fed - so let hardware feed it.
    ESP32S3_REG(RTC_CNTL_SWD_WPROTECT_REG) = RTC_CNTL_SWD_WKEY_VALUE;
    ESP32S3_REG(RTC_CNTL_SWD_CONF_REG)    |= RTC_CNTL_SWD_AUTO_FEED_EN;
    ESP32S3_REG(RTC_CNTL_SWD_WPROTECT_REG) = 0;

    ESP32S3_REG(RTC_CNTL_WDTWPROTECT_REG) = RTC_CNTL_WDT_WKEY_VALUE;
    ESP32S3_REG(RTC_CNTL_WDTCONFIG0_REG)  = 0;
    ESP32S3_REG(RTC_CNTL_WDTWPROTECT_REG) = 0;

    ESP32S3_REG(TIMG0_WDTWPROTECT_REG) = TIMG_WDT_WKEY_VALUE;
    ESP32S3_REG(TIMG0_WDTCONFIG0_REG)  = 0;
    ESP32S3_REG(TIMG0_WDTWPROTECT_REG) = 0;
}
