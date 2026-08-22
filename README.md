# Bare-metal ESP32-S3 — Waveshare ESP32-S3-Zero

A support library for this board with the framework taken away: no `idf.py`,
no CMake, no Kconfig, no `sdkconfig`, no second-stage bootloader, no FreeRTOS,
no libc. Clock, watchdogs, timing, GPIO, a USB console, and the four hardware
buses — UART, I2C, SPI and PWM — in about 2400 lines of C, comments and all.

It began as a WS2812 blink written against the ESP-IDF and then rebuilt from
the registers up, which is why there is still an LED driver in here that no
other part of the library needs. `main.c` is a skeleton that boots the chip
and stops, meant to be built on; the programs that actually do something live
in `examples/`.

```
make            # build/main.bin
make flash      # write it to the board
make monitor    # watch the console

make i2c_scan flash monitor            # build examples/i2c_scan.c instead
make list                              # what else is in examples/
```

Each image is named after the source it came from: `main.c` builds
`build/main.bin`, `examples/i2c_scan.c` builds `build/i2c_scan.bin`.

## Getting started

**You need** the Espressif Xtensa toolchain (for `xtensa-esp32s3-elf-gcc`),
`esptool`, and GNU make. Installing ESP-IDF gets you the first two; you do not
otherwise need the IDF, and nothing here includes one of its headers. On
Windows, make is not shipped — `winget install ezwinports.make`.

```
git clone https://github.com/codewithamoako/esp32s3-baremetal .
make
```

If that prints a size line and `Successfully created ESP32-S3 image`, you are
set. If it cannot find the compiler it says so and tells you what to pass.

**Finding the tools.** The Makefile looks in three places, in this order: what
you pass on the command line, then `PATH`, then a stock Windows Espressif
install. So on Linux or macOS, sourcing the IDF's `export.sh` first is enough;
on Windows the usual `C:\Espressif` install is found on its own. Anything else
gets pointed at directly:

```
make TOOLCHAIN=/path/to/xtensa-esp-elf/bin
```

**Your serial port** is the one thing that cannot be guessed. The default is
`COM6` on Windows, `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem101` on macOS —
override it if the board turns up elsewhere:

```
make flash PORT=COM7
make flash PORT=/dev/ttyUSB0
```

**Flashing** writes to offset `0x0`, which replaces the bootloader. See
[Flashing replaces the bootloader](#flashing-replaces-the-bootloader) before
you run it on a board you care about. If the board does not respond, hold
**BOOT**, tap **RST**, release BOOT to force download mode.

**Then** run `make list` and flash an example — `gpio_button` needs no wiring
at all and is the quickest way to confirm the board, the toolchain and the
flashing all work. After that, write your program in `main.c`.

## What's actually here

A clone has four things in it:

```
Makefile      compile, link, convert to a flash image, flash it
main.c        your program
esp32s3/      the support library - the chip and this board
examples/     one runnable program per peripheral
```

`examples/` is reference material, not a dependency. Delete the directory and
`make` still builds `main.c` against the library exactly as before.

Everything that knows about the chip lives in `esp32s3/`, so a program stays
readable as just the program. Write yours in `main.c`, include `board.h`
(the umbrella) plus whatever peripheral drivers you need on top — `led.h` for
the onboard RGB LED — and never touch a register directly.

| File | Job |
| --- | --- |
| `main.c` | Your program. A skeleton: boot the chip, then stop |
| `examples/` | One runnable program per peripheral — see its own README |
| `esp32s3/include/` | The headers: the whole API surface |
| `esp32s3/src/board.c` | `board_init()` — zero .bss, disable watchdogs, raise the clock |
| `esp32s3/src/clock.c` | CPU clock: read it, switch to 160 MHz, ungate peripherals |
| `esp32s3/src/console.c` | Text over USB-Serial-JTAG |
| `esp32s3/src/delay.c` | Busy-wait timing |
| `esp32s3/src/gpio.c` | Digital pins, and the GPIO matrix peripherals reach them through |
| `esp32s3/src/uart.c` | Hardware serial ports |
| `esp32s3/src/i2c.c` | I2C master |
| `esp32s3/src/spi.c` | SPI master on GP-SPI2 |
| `esp32s3/src/pwm.c` | PWM on the LEDC peripheral |
| `esp32s3/src/watchdog.c` | Disable the ROM's watchdogs |
| `esp32s3/src/led.c` | Addressable RGB LED (WS2812) bit-banging driver |
| `esp32s3/linker.ld` | Where the two segments land in SRAM |
| `Makefile` | Compile, link, convert to a flash image, flash it |

`main.c` holds only `board_init()` and a halt. A program that does
something is still just:

```c
#include "board.h"
#include "led.h"

void _start(void)
{
    board_init();              /* .bss, watchdogs, 160 MHz */
    led_init(21);
    for (;;) {
        led_set_color(LED_RED);
        delay_ms(500);
        led_set_color(LED_OFF);
        delay_ms(500);
    }
}
```

Sizes, as `text` in the linked image:

| Program | Size |
| --- | --- |
| `main.c` — boot and halt | 352 B |
| the blink above | 980 B |
| `examples/gpio_button.c` | 1.4 KB |
| a program using all four buses | 5.4 KB |

Every driver is compiled every time, but `--gc-sections` drops each one the
program never calls, so an unused bus costs nothing.

## The four buses

`board.h` pulls in UART, I2C, SPI and PWM alongside the GPIO and timing.
All four are polled — no interrupt handler, no buffering behind your back, no
scheduler. A call returns when the hardware has finished.

```c
uart_init(1, 5, 6, 115200);              /* port 1, TX 5, RX 6 */
uart_print(1, "AT\r\n");
int c = uart_read_byte(1);               /* -1 if nothing arrived */

i2c_init(0, 8, 9, 400000);               /* SDA 8, SCL 9, 400 kHz */
uint8_t who;
i2c_write_read(0, 0x68, (uint8_t[]){ 0x75 }, 1, &who, 1);

spi_init(12, 11, 13, 10, 10000000, 0);   /* SCK, MOSI, MISO, CS, 10 MHz, mode 0 */
spi_write(cmd, 1);
spi_read(id, 3);

pwm_init(0, 6, 1000, 10);                /* channel 0, GPIO6, 1 kHz, 10-bit */
pwm_set_duty(0, 512);                    /* half on */
```

Almost any pin works for any of them, because none of these peripherals is
wired to a fixed pad. A peripheral emits a numbered *signal* and a crossbar —
the GPIO matrix — decides which pad carries it. That is the whole reason the
init calls take pin numbers. `gpio_route_out()` and `gpio_route_in()` in
`gpio.h` are the two sides of that crossbar; the bus drivers are their
only expected callers.

`examples/` has a runnable program for each of these, three of which verify
themselves with nothing but a jumper wire:

```
make gpio_button  flash monitor    # BOOT button, no wiring at all
make uart_echo    flash monitor    # jumper GPIO5 to GPIO6
make spi_loopback flash monitor    # jumper GPIO11 to GPIO13
make i2c_scan     flash monitor    # a device on GPIO8/GPIO9
make pwm_fade     flash monitor    # an LED on GPIO6
```

Each header carries the limits of its own driver. The ones worth knowing up
front:

| Bus | Clocked from | Limit worth knowing |
| --- | --- | --- |
| UART | 40 MHz crystal | 8N1 only; 128-byte hardware FIFO, nothing buffered in RAM |
| I2C | 40 MHz crystal | 32 bytes per transaction, 7-bit addresses, 1 kHz–1 MHz |
| SPI | 80 MHz PLL | one device; split into 64-byte chunks with CS held across |
| PWM | 40 MHz crystal | 8 channels sharing 4 timers, so 4 distinct frequencies |

None of them is clocked from anything the CPU clock feeds, so a baud rate or a
servo pulse cannot drift because `set_cpu_160mhz()` ran. That was deliberate:
the LED timing story further down is what happens when a peripheral's timing
does depend on the CPU clock, and it is not a debugging session worth
repeating four more times.

## How it boots

The ESP32-S3 ROM looks for an image at flash offset 0. Normally that's the
ESP-IDF second-stage bootloader, which sets up the flash cache, reads the
partition table, and loads the real app. This project *is* the image at offset
0, so the chain stops there:

```
reset → ROM loader → reads image header at flash 0x0
                   → copies segment 0 to 0x3FC90000  (rodata/data)
                   → copies segment 1 to 0x40378000  (code)
                   → jumps to _start
```

Consequences worth knowing:

- **Everything runs from SRAM.** Flash is never memory-mapped, so there's no
  cache to configure and no XIP. That's why an `IRAM_ATTR` equivalent is
  unnecessary here — all the code is already in IRAM.
- **We run on the ROM's stack.** ESP-IDF's own bootloader does exactly this.
  The ROM stack sits at `0x3FCE9710`, well above anything this image uses.
- **We keep the ROM's exception vectors.** `VECBASE` is left alone, which is
  what makes Xtensa register-window overflow and underflow keep working
  without a single handler of our own. It also means `_start` can be a plain
  C function rather than assembly.
- **Nothing zeroes `.bss`.** The image only carries sections with contents,
  so `_start` zeroes it before anything reads it.

## The two things ESP-IDF was doing for you

**Watchdogs.** Before jumping into the image at offset 0, the ROM arms the RTC
watchdog and timer-group 0's watchdog in "flash boot" mode — insurance against
a second-stage bootloader that hangs. We are that second-stage bootloader now.
Leave them armed and the board reboots every few hundred milliseconds.
`disable_watchdogs()` turns both off and puts the super watchdog
(which can't be disabled, only fed) into hardware auto-feed.

**The CPU clock.** The ROM leaves the CPU on the crystal divided by two —
20 MHz — and that is not fast enough to bit-bang an addressable LED. This is
worth understanding, because it fails in a way that looks like something else
entirely.

The cycle-count poll in `led_write()` costs a fixed ~9 cycles per pass, so
every pulse overshoots by that much. The overshoot is a *cycle* count, so what
it costs in nanoseconds depends on the clock — and the whole protocol comes
down to telling a 350 ns pulse from an 800 ns one. Measured on the board:

| CPU | a 350 ns "zero" comes out at | |
| --- | --- | --- |
| 20 MHz | 1150 ns | reads as a one |
| 40 MHz | 575 ns | marginal |
| 160 MHz | 406 ns | correct |

A misread bit is a *wrong colour*, not a dark LED — so the symptom points at
the palette when the cause is the clock.

`set_cpu_160mhz()` fixes it in three register writes. That works because
the ROM has already started the PLL to clock the flash it read this image
from, so there's no analog bring-up to do — which is the part that would have
meant driving undocumented registers over the internal I2C bus. The order
matches ESP-IDF's `rtc_clk_cpu_freq_to_pll_mhz()`: frequency, divider, source.

`read_cpu_mhz()` then reads the result back rather than trusting a constant,
so the timing can't drift out of sync with the setting the way
`CPU_MHZ`-versus-`sdkconfig` can in the IDF project. `boot_cpu_mhz()` keeps
the speed the ROM handed over, so a program can print both and see the switch
happen: `cpu 20 -> 160 MHz`.

If you ever hit a boot path where the PLL *isn't* already running, that switch
hangs the CPU. The fallback is the crystal undivided — 40 MHz, marginal but
alive:

```c
uint32_t v = ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG);
v &= ~0x3FFu;         ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG) = v;  /* divide by 1  */
v &= ~(0x3u << 10);   ESP32S3_REG(SYSTEM_SYSCLK_CONF_REG) = v;  /* source XTAL  */
```

## Memory map

Internal SRAM is reachable from two buses at addresses `0x6F0000` apart:
instructions are fetched through `0x403xxxxx`, data is read and written
through `0x3FCxxxxx`. Same physical memory — so the two regions in
`esp32s3/linker.ld` must not overlap once that offset is applied.

| Region | Address | Also known as | Holds |
| --- | --- | --- | --- |
| `iram` | `0x40378000` + 32K | phys `0x3FC88000` | `.text`, literal pools |
| `dram` | `0x3FC90000` + 256K | — | `.rodata`, `.data`, `.bss` |
| — | `0x3FCD7E00` | — | ROM's data starts; stay below |
| — | `0x3FCE9710` | — | ROM's stack, which we borrow |

## Build settings

Everything is a make variable with a sensible default, so there is no config
file and nothing to generate:

| Variable | Default | What it is |
| --- | --- | --- |
| `TOOLCHAIN` | found automatically | Directory holding `xtensa-esp32s3-elf-gcc` |
| `ESPTOOL` | found automatically | `esptool` used to build and write the image |
| `PYTHON` | found automatically | Only used by `make monitor` |
| `PORT` | per platform | Serial port the board appears as |
| `BAUD` | `460800` | Flashing speed |
| `FLASH_MODE` / `FLASH_FREQ` / `FLASH_SIZE` | `dio` / `80m` / `4MB` | How the ROM reads the image |

```
make TOOLCHAIN=D:/some/other/bin PORT=COM7 flash
```

### Windows: keep the toolchain path space-free

Install the toolchain somewhere like `C:\Espressif`, not
`C:\Users\<name>\.espressif`. A space anywhere in the toolchain path makes
build systems fall back to DOS 8.3 short names, which mangles the compiler's
own filename from `xtensa-esp32s3-elf-gcc.exe` to something like
`XT34AB~1.EXE`. The multi-target Xtensa driver picks its chip config from its
own filename, so once mangled it fails with:

```
cc1.exe: fatal error: Both 'XTENSA_GNU_CONFIG' and "-dynconfig=" specified
but pointed different files
```

For the same reason, run `make` from inside the project directory rather than
with `make -C`: relative paths keep the project's own path, which may well
contain a space, out of every rule.

## Flashing replaces the bootloader

`make flash` writes to offset `0x0`, which is where the ESP-IDF bootloader
lives. After this the board runs only this image. Any ESP-IDF app that was on
the board is gone with it, and won't come back until you reflash that project
with `idf.py flash`, which rewrites the bootloader, the partition table, and
the app together.

If the board doesn't respond, hold **BOOT**, tap **RST** (or replug), release
BOOT to force download mode. The single USB-C port is USB-Serial-JTAG, so
there's no auto-reset circuit doing it for you.

## Console

`console_print()` writes to the USB-Serial-JTAG FIFO directly — two registers,
no driver, no UART, no pin. The ROM has already enumerated the port, so output
appears on the same COM port you flash over, and `make monitor` shows it.

This is not the UART. `console_print()` goes out of the USB-C socket to your
PC; `uart_print()` goes out of a pin to whatever is wired to it. The two have
nothing to do with each other.

`main.c` prints nothing, so a default build is silent by design. The
examples all print — `make i2c_scan flash monitor` is a quick way to see it
working:

```
i2c port 0 on sda=8 scl=9 at 100 kHz
scanning 0x08..0x77
  found 0x68  reg[0] = 0x19
1 device
```

Writes are dropped rather than blocking when no host is reading, so an
unattended board keeps running at full speed.

That dropping has one consequence worth knowing: **anything printed in the
first moment after reset is lost.** `_start` runs microseconds after reset,
but the USB port takes about a second to enumerate with the host, and a
terminal opened later still misses whatever came before it. If a program's
startup lines never appear, tap **RST** with `make monitor` already running.

## Changing things

Every peripheral takes its settings as arguments to its `*_init()` call —
pins, baud rate, bus speed, SPI mode, PWM frequency — so there is no
configuration file and nothing to regenerate. The headers in `esp32s3/include/`
document the range each one accepts.

The LED driver is the one exception, because it has wire timings that come
from a datasheet rather than from you:

- **Colour** — `LED_RGB(r, g, b)`, or the `LED_RED` / `LED_GREEN` /
  `LED_BLUE` / `LED_WHITE` presets in `led.h`. Plain RGB order; the GRB order
  the part wants on the wire is `led_write()`'s problem, not the caller's.
- **Brightness** — the numbers in those macros, 0 to 255 per channel. The
  presets use 10, which is already bright enough to be unpleasant to look at.
- **Pin** — the argument to `led_init()`. `PIN_LED` is GPIO21 on this board.
- **Wire timing** — `LED_T0H_NS` and the four constants beside it at the top
  of `esp32s3/src/led.c`, in nanoseconds, from the WS2812B datasheet.
