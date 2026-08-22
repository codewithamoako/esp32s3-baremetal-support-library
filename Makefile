# Bare-metal support library for the Waveshare ESP32-S3-Zero.
#
#   make            build/main.bin
#   make flash      write it to the board at flash offset 0
#   make monitor    watch the USB-Serial-JTAG console
#   make size       section sizes
#   make clean
#
# Naming a program from examples/ builds that instead of main.c, and
# works with every target above:
#
#   make list                          what is in examples/
#   make i2c_scan flash monitor        build it, flash it, watch it
#   make EXAMPLE=i2c_scan flash        the same thing, spelled out
#
# Only two tools are borrowed from the Espressif install: the xtensa compiler
# and esptool. Nothing here includes an ESP-IDF header or links an IDF library.
#
# ---------------------------------------------------------------------------
# Finding those tools
#
# Three places are tried, in order, so that a fresh clone builds on a machine
# that has never seen this project:
#
#   1. TOOLCHAIN=, ESPTOOL=, PYTHON= on the command line. Always wins.
#   2. PATH - what `. $IDF_PATH/export.sh` leaves behind on Linux and macOS.
#   3. A stock Windows Espressif install, matched by glob so that the version
#      in the directory name does not have to be known in advance.
#
# The PATH step is skipped on native Windows make, where $(shell) may be cmd
# rather than sh and a probe would either print noise or leave a stray file
# behind. Step 3 covers the normal Windows install; anything else needs
# TOOLCHAIN= spelled out.
#
# If no compiler turns up, make stops with an explanation rather than letting
# a "command not found" surface three levels down.
#
# On Windows, keep the toolchain somewhere space-free such as C:\Espressif
# rather than C:\Users\<name>\.espressif. A space anywhere in the toolchain
# path makes build systems fall back to 8.3 short names, which mangles the
# compiler's own filename and breaks its target detection. For the same
# reason, run make from inside the project directory rather than with -C, so
# that the project's own path never has to reach a rule.
# ---------------------------------------------------------------------------

ifeq ($(MAKE_HOST),Windows32)
  on_path =
else
  on_path = $(shell command -v $(1))
endif

# A glob can match several installed versions; any of them builds this.
espressif_dir = $(dir $(firstword $(wildcard $(1))))

GCC := xtensa-esp32s3-elf-gcc

ifneq ($(TOOLCHAIN),)
  TOOLCHAIN_BIN := $(TOOLCHAIN)/
else ifneq ($(call on_path,$(GCC)),)
  TOOLCHAIN_BIN :=
else
  TOOLCHAIN_BIN := $(call espressif_dir,C:/Espressif/tools/xtensa-esp-elf/*/xtensa-esp-elf/bin/$(GCC).exe)
  ifeq ($(TOOLCHAIN_BIN),)
    TOOLCHAIN_MISSING := 1
  endif
endif

ifdef TOOLCHAIN_MISSING
define toolchain_help

  Cannot find the Xtensa compiler ($(GCC)).

  Install the ESP-IDF toolchain, then either put it on PATH -

      Linux, macOS   . $$IDF_PATH/export.sh
      Windows        & $$env:IDF_PATH\export.ps1

  - or point make straight at the directory holding it:

      make TOOLCHAIN=/path/to/xtensa-esp-elf/bin

  Only the compiler is needed to build. Flashing also wants esptool.

endef
  $(error $(toolchain_help))
endif

CC   := $(TOOLCHAIN_BIN)$(GCC)
SIZE := $(TOOLCHAIN_BIN)xtensa-esp32s3-elf-size

# esptool and python are only needed by `flash` and `monitor`, so a missing one
# is not worth stopping a build over - it fails at the point it is used.
ESPTOOL_WIN := $(call espressif_dir,C:/Espressif/tools/python/*/venv/Scripts/esptool.exe)
PYTHON_WIN  := $(call espressif_dir,C:/Espressif/tools/python/*/venv/Scripts/python.exe)

# $(strip) because a line continuation below would otherwise leave a leading
# space inside the command name, which survives all the way to the shell.
ESPTOOL ?= $(strip $(if $(call on_path,esptool.py),esptool.py,\
             $(if $(ESPTOOL_WIN),$(ESPTOOL_WIN)esptool.exe,esptool)))
PYTHON  ?= $(strip $(if $(PYTHON_WIN),$(PYTHON_WIN)python.exe,\
             $(if $(call on_path,python3),python3,python)))

# The serial port the board shows up as. There is no reliable way to detect
# this, so it is a guess per platform and the first thing to override.
ifeq ($(MAKE_HOST),Windows32)
  PORT ?= COM6
else ifeq ($(shell uname -s),Darwin)
  PORT ?= /dev/cu.usbmodem101
else
  PORT ?= /dev/ttyACM0
endif

BAUD ?= 460800

# Flash settings only affect how the ROM reads this image; nothing executes
# from flash once it is loaded.
FLASH_MODE ?= dio
FLASH_FREQ ?= 80m
FLASH_SIZE ?= 4MB

# -mlongcalls and -mtext-section-literals are the Xtensa essentials: the first
# lets calls reach anywhere, the second keeps each function's literal pool next
# to the code that uses it. -Iesp32s3/include is where board.h and the other
# library headers live.
CFLAGS := -std=c11 -Os -g -Wall -Wextra \
          -Iesp32s3/include \
          -mlongcalls -mtext-section-literals \
          -ffreestanding -ffunction-sections -fdata-sections

# The program to build: main.c unless an example from examples/ is named.
#
# An example can be named either way round - `make i2c_scan` or
# `make EXAMPLE=i2c_scan`. The bare name is what everyone types first, so it
# has to pick the program up for the *whole* command line: otherwise
# `make i2c_scan flash` would build one program and flash a different one.
# That is why the name is turned into EXAMPLE here rather than being handled
# by a target of its own further down.
EXAMPLES := $(basename $(notdir $(wildcard examples/*.c)))
EXAMPLE  ?= $(firstword $(filter $(EXAMPLES),$(MAKECMDGOALS)))

ifeq ($(EXAMPLE),)
  PROGRAM := main.c
  NAME    := main
else
  PROGRAM := examples/$(EXAMPLE).c
  NAME    := $(EXAMPLE)
endif

# Every image is named after the source file it was built from, so main.c
# gives build/main.bin and examples/i2c_scan.c gives build/i2c_scan.bin. That
# is not cosmetic: with one fixed output name, switching from one example to
# another and back would leave make comparing an old object against a newer
# image, decide there was nothing to do, and flash the *previous* example.
# Separate names cannot go stale.
ELF := build/$(NAME).elf
BIN := build/$(NAME).bin

# The support library is everything in esp32s3/src - the chip and this board,
# with no program in it. Whichever program gets built links against all of it,
# and --gc-sections drops the parts that program never calls.
LDSCRIPT := esp32s3/linker.ld
LIB_SRCS := $(wildcard esp32s3/src/*.c)
LIB_OBJS := $(patsubst esp32s3/src/%.c,build/%.o,$(LIB_SRCS))
PROG_OBJ := build/prog_$(NAME).o
OBJS     := $(LIB_OBJS) $(PROG_OBJ)

# -lgcc supplies the handful of helpers gcc emits for division; it is not libc.
LDFLAGS := -nostdlib -T $(LDSCRIPT) -Wl,--gc-sections -Wl,-Map=build/$(NAME).map
LDLIBS  := -lgcc

# Native Windows make has no shell to run rm or mkdir through, and cmd's
# versions are builtins rather than executables - so call cmd explicitly.
# MAKE_HOST is "Windows32" only for a native build; MSYS, Cygwin and Linux
# makes all report something else and get the POSIX commands.
ifeq ($(MAKE_HOST),Windows32)
  MKDIR_BUILD := cmd /c if not exist build mkdir build
  CLEAN_BUILD := cmd /c if exist build rmdir /s /q build
else
  MKDIR_BUILD := mkdir -p build
  CLEAN_BUILD := rm -rf build
endif

.PHONY: all flash monitor size clean list $(EXAMPLES)
.DEFAULT_GOAL := all

all: $(BIN)

# Naming an example is a request to build it; EXAMPLE was already set from the
# goal list above, so `all` is building the right program by the time we
# get here and there is nothing left for these to do.
$(EXAMPLES): all

# No angle brackets or quotes in this recipe: it has to survive both cmd and
# sh, and each of them mangles a different set of punctuation.
list:
	@echo Examples: $(EXAMPLES)
	@echo Usage: make NAME flash monitor - e.g. make i2c_scan flash monitor

$(BIN): $(ELF)
	$(ESPTOOL) --chip esp32s3 elf2image \
	    --flash-mode $(FLASH_MODE) --flash-freq $(FLASH_FREQ) --flash-size $(FLASH_SIZE) \
	    -o $@ $<

$(ELF): $(OBJS) $(LDSCRIPT)
	$(CC) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@
	@$(SIZE) $@

build/%.o: esp32s3/src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG_OBJ): $(PROGRAM) | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@$(MKDIR_BUILD)

# Offset 0 is where the ROM looks for its first image. Writing here replaces
# whatever bootloader was there, so an ESP-IDF app on this board stops booting
# until you flash it again with idf.py.
flash: $(BIN)
	$(ESPTOOL) --chip esp32s3 --port $(PORT) --baud $(BAUD) write-flash 0x0 $<

monitor:
	$(PYTHON) -m serial.tools.miniterm --raw $(PORT) 115200

size: $(ELF)
	$(SIZE) -A $<

clean:
	@$(CLEAN_BUILD)
