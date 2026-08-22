/*
 * ADC: read a potentiometer and print where it is pointing.
 *
 *   make adc_read flash monitor
 *
 * A potentiometer is the easiest analog part to see working, but nothing here
 * is specific to one: an LDR, a thermistor, a joystick axis, a force sensor
 * and a slider all arrive the same way - as a voltage on a pin - and this
 * example reads any of them unchanged.
 *
 * Attenuation is the one setting to get right. The converter itself only
 * measures up to about 950 mV; the attenuator divides the pin down into that
 * range, so it decides what "full scale" means at the pad. A pot across the
 * 3V3 rail swings the whole way, which is ADC_ATTEN_12DB. Choosing a range
 * the signal overruns pins the reading at ADC_MAX and you lose the top of the
 * travel; choosing one far too wide throws away resolution you had.
 *
 * The reading is a raw count, 0..4095, not a voltage. Espressif calibrates
 * each chip at the factory and stores the correction in eFuse, which this
 * library does not read - so counts are comparable to each other on this
 * board but are not millivolts. For a pot, a threshold, or anything watching
 * a change rather than an absolute level, that costs nothing.
 *
 * Two things smooth the output. Averaging several conversions cuts the SAR's
 * own noise, which is a few counts either way and would otherwise make a
 * still pot look busy. The deadband then suppresses the rest: without it a
 * reading resting between two counts flickers between them forever and fills
 * the console with a pot nobody is touching.
 *
 * Both ends of the range are non-linear on this chip, so a pot turned fully
 * down often reads a little above 0 and fully up stops a little short of
 * 4095. That is the converter, not the wiring.
 */

#include "esp32s3.h"
#include "esp32s3_adc.h"
#include "board_pins.h"

// Any of GPIO1..GPIO10; the rest of the chip's pins have no ADC1 channel.
#define PIN_POT         PIN_GPIO1

#define SAMPLES         8       // conversions averaged per reading
#define DEADBAND        24      // counts of movement before it is worth saying
#define BAR_WIDTH       40      // characters across for the full range
#define POLL_MS         50

// One reading, averaged. Each conversion takes a few microseconds, so eight
// of them still cost far less than the delay between readings.
static uint16_t read_pot(void)
{
    uint32_t total = 0;

    for (uint32_t i = 0; i < SAMPLES; i++) {
        total += adc_read(PIN_POT);
    }

    return (uint16_t)(total / SAMPLES);
}

// A moving bar makes the shape of the travel obvious - where a pot is coarse,
// where it is fine, and whether it reaches both ends.
static void print_bar(uint16_t value)
{
    uint32_t filled = ((uint32_t)value * BAR_WIDTH) / (ADC_MAX + 1);

    console_print_char('[');
    for (uint32_t i = 0; i < BAR_WIDTH; i++) {
        console_print_char(i < filled ? '#' : ' ');
    }
    console_print("] ");
    console_print_u32(value);
    console_print("\r\n");
}

void _start(void)
{
    board_init();

    if (!adc_init(PIN_POT, ADC_ATTEN_12DB)) {
        console_print("GPIO");
        console_print_u32(PIN_POT);
        console_print(" has no ADC1 channel - use GPIO1..GPIO10\r\n");
        for (;;) {
        }
    }

    console_print("turn the pot\r\n");

    uint16_t last = read_pot();
    print_bar(last);

    for (;;) {
        uint16_t value = read_pot();
        uint16_t moved = value > last ? value - last : last - value;

        if (moved >= DEADBAND) {
            last = value;
            print_bar(value);
        }

        delay_ms(POLL_MS);
    }
}
