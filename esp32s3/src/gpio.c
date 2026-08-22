/*
 * Pins: plain digital I/O, and the GPIO matrix that peripherals reach pads
 * through.
 */

#include "esp32s3_gpio.h"

// The IO MUX word for one pad: which of the pad's own functions is selected,
// how hard it drives, and whether the input buffer and pulls are on.
static void io_mux_config(uint32_t pin, gpio_pull_t pull, int input_enable)
{
    uint32_t cfg = (IO_MUX_FUNC_GPIO << IO_MUX_MCU_SEL_S)   // pad <-> GPIO matrix
                 | (2u << IO_MUX_FUN_DRV_S);                // ~20 mA

    if (input_enable) {
        cfg |= 1u << IO_MUX_FUN_IE_S;
    }
    if (pull == GPIO_PULLUP) {
        cfg |= 1u << IO_MUX_FUN_PU_S;
    } else if (pull == GPIO_PULLDOWN) {
        cfg |= 1u << IO_MUX_FUN_PD_S;
    }

    ESP32S3_REG(IO_MUX_GPIO_REG(pin)) = cfg;
}

// Output enable lives in a write-1-to-set register pair, with a second pair
// for the pins above 31.
static void output_enable(uint32_t pin, int enable)
{
    if (pin < 32) {
        ESP32S3_REG(enable ? GPIO_ENABLE_W1TS_REG : GPIO_ENABLE_W1TC_REG) = 1u << pin;
    } else {
        ESP32S3_REG(enable ? GPIO_ENABLE1_W1TS_REG : GPIO_ENABLE1_W1TC_REG) = 1u << (pin - 32);
    }
}

// gpio_set_high() and gpio_set_low() are bank 0 only, for the sake of the
// LED's bit loop. Configuration is not on that path and can afford the check.
static void drive(uint32_t pin, int level)
{
    if (pin < 32) {
        ESP32S3_REG(level ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG) = 1u << pin;
    } else {
        ESP32S3_REG(level ? GPIO_OUT1_W1TS_REG : GPIO_OUT1_W1TC_REG) = 1u << (pin - 32);
    }
}

void gpio_config_output(uint32_t pin)
{
    io_mux_config(pin, GPIO_FLOAT, 0);

    // GPIO matrix: take the pad's level straight from GPIO_OUT_REG bit <pin>.
    ESP32S3_REG(GPIO_FUNC_OUT_SEL_CFG_REG(pin)) = GPIO_SIG_OUT_IDX;

    // Start low, then enable the output driver.
    gpio_set_low(pin);
    output_enable(pin, 1);
}

void gpio_config_input(uint32_t pin, gpio_pull_t pull)
{
    io_mux_config(pin, pull, 1);
    output_enable(pin, 0);
}

void gpio_config_open_drain(uint32_t pin, gpio_pull_t pull)
{
    io_mux_config(pin, pull, 1);
    ESP32S3_REG(GPIO_FUNC_OUT_SEL_CFG_REG(pin)) = GPIO_SIG_OUT_IDX;
    ESP32S3_REG(GPIO_PIN_REG(pin)) |= GPIO_PIN_PAD_DRIVER;

    // Start released rather than pulling the shared line down the moment the
    // pad is enabled - on an open-drain bus a high is the absence of a low.
    drive(pin, 1);
    output_enable(pin, 1);
}

int gpio_read(uint32_t pin)
{
    if (pin < 32) {
        return (int)((ESP32S3_REG(GPIO_IN_REG) >> pin) & 1u);
    }
    return (int)((ESP32S3_REG(GPIO_IN1_REG) >> (pin - 32)) & 1u);
}

void gpio_route_out(uint32_t pin, uint32_t signal)
{
    io_mux_config(pin, GPIO_FLOAT, 0);

    // The pad still needs its output driver enabled; what the signal decides
    // is the level, not whether the pad is an output at all.
    ESP32S3_REG(GPIO_FUNC_OUT_SEL_CFG_REG(pin)) = signal;
    output_enable(pin, 1);
}

void gpio_route_in(uint32_t pin, uint32_t signal, gpio_pull_t pull)
{
    io_mux_config(pin, pull, 1);
    output_enable(pin, 0);

    // Without GPIO_SIG_IN_SEL the field below means "hold this signal at a
    // constant level" rather than "read it off this pad".
    ESP32S3_REG(GPIO_FUNC_IN_SEL_CFG_REG(signal)) =
        GPIO_SIG_IN_SEL | (pin & GPIO_FUNC_IN_SEL_M);
}

void gpio_route_open_drain(uint32_t pin, uint32_t signal, gpio_pull_t pull)
{
    // Input buffer on: on an open-drain bus the line's real level is whatever
    // it settles at, and the controller has to be able to see that.
    io_mux_config(pin, pull, 1);

    // PAD_DRIVER turns the push-pull driver into an open-drain one: a low
    // still pulls the line down, a high simply lets go of it.
    ESP32S3_REG(GPIO_PIN_REG(pin)) |= GPIO_PIN_PAD_DRIVER;

    ESP32S3_REG(GPIO_FUNC_OUT_SEL_CFG_REG(pin)) = signal;
    ESP32S3_REG(GPIO_FUNC_IN_SEL_CFG_REG(signal)) =
        GPIO_SIG_IN_SEL | (pin & GPIO_FUNC_IN_SEL_M);
    output_enable(pin, 1);
}
