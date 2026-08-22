/*
 * Hardware UART - the ordinary asynchronous serial port.
 *
 * Not to be confused with console.h, which also prints text but does it over
 * the USB-C port and cannot talk to anything except a host PC. This is the
 * peripheral you use to reach another *device*: a GPS module, a modem, a
 * second microcontroller, a level shifter on a bench cable.
 *
 * Three ports, and the GPIO matrix means each can appear on almost any pin.
 * The board labels GPIO43/GPIO44 "TX"/"RX" because that is where the chip's
 * UART0 lands by default, but nothing stops you putting UART1 on GPIO5/GPIO6.
 *
 *     uart_init(1, 5, 6, 115200);
 *     uart_print(1, "AT\r\n");
 *
 *     int c = uart_read_byte(1);
 *     if (c >= 0) { ... }
 *
 * Polled, not interrupt-driven: nothing is buffered in RAM behind your back,
 * so bytes that arrive while you are not calling uart_read_byte() live in the
 * hardware's 128-byte FIFO and are lost once it fills. At 115200 baud that is
 * about 11 ms of silence you can afford between reads.
 *
 * 8 data bits, no parity, 1 stop bit. That is the format almost everything
 * uses, and the ones that don't are rare enough not to earn an argument here.
 *
 * The baud rate is generated from the 40 MHz crystal rather than from a clock
 * derived from the CPU, so it stays correct across set_cpu_160mhz().
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

// Pass instead of a pin number to leave that half of the port unwired - a
// transmit-only or receive-only link.
#define UART_PIN_NONE   0xFFFFFFFFu

// Sets a port up as 8N1 at the given baud rate and puts it on those pins.
// Rates from about 40 baud to several megabaud are reachable; the divider is
// a fraction, so common rates land within a fraction of a percent.
// - port: 0, 1 or 2
// - tx_pin: GPIO to transmit on, or UART_PIN_NONE
// - rx_pin: GPIO to receive on, or UART_PIN_NONE
// - baud: bits per second, e.g. 115200
// returns: nothing
void uart_init(uint32_t port, uint32_t tx_pin, uint32_t rx_pin, uint32_t baud);

// Sends one byte, waiting for room in the transmit FIFO first.
// - port: 0, 1 or 2
// - byte: the byte to send
// returns: nothing
void uart_write_byte(uint32_t port, uint8_t byte);

// Sends a block of bytes.
// - port: 0, 1 or 2
// - data: what to send
// - len: how many bytes
// returns: nothing
void uart_write(uint32_t port, const void *data, uint32_t len);

// Sends a null-terminated string, without the terminator.
// - port: 0, 1 or 2
// - text: the string to send
// returns: nothing
void uart_print(uint32_t port, const char *text);

// How many bytes are waiting in the receive FIFO.
// - port: 0, 1 or 2
// returns: 0 to 128
uint32_t uart_rx_available(uint32_t port);

// Takes one byte from the receive FIFO if there is one. Never blocks.
// - port: 0, 1 or 2
// returns: the byte (0 to 255), or -1 if nothing has arrived
int uart_read_byte(uint32_t port);

// Takes whatever has arrived, up to a limit. Never blocks, so a short return
// means the rest has not turned up yet, not that the sender stopped.
// - port: 0, 1 or 2
// - buf: where to put the bytes
// - max_len: the size of buf
// returns: how many bytes were read, possibly 0
uint32_t uart_read(uint32_t port, void *buf, uint32_t max_len);

// Waits until the last bit of the last queued byte has left the pin. Worth
// calling before you cut power, reset, or reconfigure the port.
// - port: 0, 1 or 2
// returns: nothing
void uart_drain(uint32_t port);

#endif // ESP32S3_UART_H
