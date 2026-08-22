/*
 * ADC1 one-shot conversions: one call, one 12-bit reading, no DMA or scan list.
 *
 * Readings are raw. Espressif's per-chip calibration lives in eFuse and needs
 * a driver of its own, so a count is a count rather than a millivolt - which
 * is invisible to anything measuring a change rather than an absolute level.
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// What reads as full scale. The ends of each range are non-linear, so a
// reading pinned at 0 or ADC_MAX means the range is wrong, not the signal.
typedef enum {
    ADC_ATTEN_0DB   = 0,        // up to ~950 mV
    ADC_ATTEN_2_5DB = 1,        // up to ~1250 mV
    ADC_ATTEN_6DB   = 2,        // up to ~1750 mV
    ADC_ATTEN_12DB  = 3,        // up to ~3100 mV
} adc_atten_t;

#define ADC_MAX     4095

// Which ADC1 channel a pin is wired to, or -1 if it has none.
int adc_channel_for_pin(uint32_t pin);

// Powers the converter up and hands the pad to the analog mux. The pin stops
// being a GPIO until something reconfigures it.
// - pin: GPIO1..GPIO10
// returns: 1 once configured, 0 if the pin has no ADC1 channel
int adc_init(uint32_t pin, adc_atten_t atten);

// One conversion, spinning until the hardware is done - a few microseconds.
// returns: 0..ADC_MAX, or 0 if the pin has no ADC1 channel
uint16_t adc_read(uint32_t pin);

#endif // ESP32S3_ADC_H
