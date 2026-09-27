#include <stdint.h>

#include "minemu/platform.h"
#include "minemu/uart.h"

void minemu_uart_putc(char byte) {
    while ((MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) == 0) {
    }
    MINEMU_UART0->tx_data = (uint32_t)(uint8_t)byte;
}

void minemu_uart_puts(const char *text) {
    while (*text != '\0') {
        minemu_uart_putc(*text++);
    }
}
