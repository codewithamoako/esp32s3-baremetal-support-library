# Examples

One program per peripheral. Each replaces `main.c` for the build — just
name it:

```
make list                       # what is here
make i2c_scan                   # build it
make i2c_scan flash monitor     # build it, flash it, watch it
```

`make EXAMPLE=i2c_scan` is the same thing spelled out, if you prefer. Either
form works with every target — `all`, `flash`, `size`, `clean`.

Outputs are named after the program (`build/i2c_scan.bin`), so switching
between examples never flashes the previous one by mistake. Plain `make` still
builds `main.c` into `build/main.bin`.

Every example prints to the USB-C console, so `make monitor` shows what it is
doing. That console is the USB-Serial-JTAG peripheral, which has nothing to do
with the UART on the pins — `console_print()` talks to your PC, `uart_print()`
talks to whatever is wired to GPIO5.

| Example | Shows | Needs |
| --- | --- | --- |
| [gpio_button.c](gpio_button.c) | Input pins, pull-ups, debouncing | nothing |
| [uart_echo.c](uart_echo.c) | Serial TX/RX, FIFO, baud rate | one jumper wire |
| [i2c_scan.c](i2c_scan.c) | Bus scan, register reads, error returns | an I2C device + 2 pull-ups |
| [spi_loopback.c](spi_loopback.c) | Full duplex, chunk splitting, CS | one jumper wire |
| [pwm_fade.c](pwm_fade.c) | Duty cycles, frequency/resolution trade | an LED, or a servo |

Three of the five verify themselves and need no parts, which is the point:
when something does not work, you want to be sure the bus is right before you
start suspecting the device.

## Wiring

**gpio_button** — nothing. The BOOT button and the RGB LED are both onboard.

**uart_echo** — one wire, GPIO5 to GPIO6. That ties TX to RX so the port talks
to itself; the example reports whether what it sent came back. Remove the wire
and attach a real device to those pins to watch its traffic instead.

**spi_loopback** — one wire, GPIO11 (MOSI) to GPIO13 (MISO). SPI shifts a bit
out and a bit in on the same edge, so with those tied together every byte sent
arrives back in the same transfer. Tests one byte, one full 64-byte hardware
transfer, and a 200-byte transfer that the driver has to split.

**i2c_scan** — a device on GPIO8 (SDA) and GPIO9 (SCL), plus 4.7k resistors
from each line to 3V3. The internal pull-ups the driver enables are around
45k, which sometimes carries one device on a short breadboard bus at 100 kHz
and is not enough beyond that.

**pwm_fade** — an LED and a 330R resistor from GPIO6 to ground, and
optionally a servo signal wire on GPIO7. The onboard RGB LED cannot be used:
it is a WS2812, which wants a serial data stream rather than a duty cycle.

## Pin choices

The pins above are arbitrary. Nearly every one of these peripherals can appear
on nearly any pad, because none of them is hard-wired: a peripheral emits a
numbered signal and the GPIO matrix decides which pad carries it. Change the
`#define`s at the top of an example and it will work the same.

Two constraints on this board: GPIO33–GPIO37 are not broken out (they are the
PSRAM interface), and GPIO19/GPIO20 are the USB port. `board_pins.h` has the
full list.
