/*
 * Linker script for a bare-metal ESP32-S3 image loaded by the ROM from
 * flash offset 0.
 *
 * The ROM loader reads the image header, copies each segment to its load
 * address in SRAM, and jumps to the entry point. There is no second-stage
 * bootloader and flash is never memory-mapped, so every byte we care about
 * has to live in one of the two regions below.
 *
 * Internal SRAM is reachable from two buses at addresses 0x6F0000 apart:
 * instructions must be fetched through 0x403xxxxx, data is read and written
 * through 0x3FCxxxxx. They are the same physical memory, so the two regions
 * here must not overlap once that offset is applied:
 *
 *   iram  0x40378000..0x40380000  ==  physical 0x3FC88000..0x3FC90000
 *   dram  0x3FC90000..0x3FCD0000  (the next 256K up, no overlap)
 *
 * The upper bound matters. The ROM keeps its own data from 0x3FCD7E00 and
 * its stack at 0x3FCE9710 - and we run on that stack - so nothing here may
 * climb past 0x3FCD7E00. Ending dram at 0x3FCD0000 leaves a margin.
 */

ENTRY(_start)

MEMORY
{
  iram (rwx) : ORIGIN = 0x40378000, LENGTH = 32K
  dram (rw)  : ORIGIN = 0x3FC90000, LENGTH = 256K
}

SECTIONS
{
  /*
   * Xtensa keeps constants in literal pools that L32R reaches with a
   * negative offset, so every .literal must be linked below the .text that
   * refers to it. Listing the literals first guarantees that.
   */
  .text : ALIGN(4)
  {
    KEEP(*(.text._start))
    *(.literal .literal.* .text .text.*)
    . = ALIGN(4);
  } > iram

  .rodata : ALIGN(4)
  {
    *(.rodata .rodata.*)
    . = ALIGN(4);
  } > dram

  .data : ALIGN(4)
  {
    *(.data .data.*)
    . = ALIGN(4);
  } > dram

  /*
   * NOLOAD keeps .bss out of the flash image - it has no contents to carry.
   * _start zeroes it using these two symbols.
   */
  .bss (NOLOAD) : ALIGN(4)
  {
    __bss_start = .;
    *(.bss .bss.* COMMON)
    . = ALIGN(4);
    __bss_end = .;
  } > dram

  /DISCARD/ : { *(.comment) *(.note.*) *(.xt.*) }
}
