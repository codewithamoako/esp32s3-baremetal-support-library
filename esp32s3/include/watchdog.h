/*
 * The watchdogs the ROM arms on our behalf.
 *
 * Before jumping into the image at flash offset 0 the ROM starts the RTC
 * watchdog and timer-group 0's watchdog in "flash boot" mode, as insurance
 * against a second-stage bootloader that hangs. An image written straight to
 * offset 0 *is* that second-stage bootloader, so it has to take them down or
 * the board reboots every few hundred milliseconds.
 *
 * Call disable_watchdogs() first thing in _start().
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

// Stops the RTC and timer-group 0 watchdogs, and puts the super watchdog
// into hardware auto-feed. Call first thing in _start().
// returns: nothing
void disable_watchdogs(void);

#endif // ESP32S3_WATCHDOG_H
