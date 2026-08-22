/*
 * Chip bring-up: zeroes .bss, disables the watchdogs, and raises the CPU
 * clock. board_init() ties it together in the right order.
 */

#include "board.h"

// Placed by linker.ld around the .bss section.
extern uint32_t __bss_start, __bss_end;

static uint32_t startup_cpu_mhz;

static void clear_bss(void)
{
    for (uint32_t *word = &__bss_start; word < &__bss_end; word++) {
        *word = 0;
    }
}

void board_init(void)
{
    clear_bss();

    uint32_t rom_cpu_mhz = read_cpu_mhz();

    disable_watchdogs();
    set_cpu_160mhz();

    startup_cpu_mhz = rom_cpu_mhz;  // after clear_bss(), or it would be wiped
}

uint32_t boot_cpu_mhz(void)
{
    return startup_cpu_mhz;
}
