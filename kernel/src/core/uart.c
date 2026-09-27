#include <stdint.h>

#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/uart.h"

#define RX_BUFFER_SIZE 256u

static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;
static volatile uint32_t rx_overflow;

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

void minemu_uart_enable_rx(void) {
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    MINEMU_INTERRUPT->enable = UINT32_C(1) << MINEMU_IRQ_UART0;
}

void minemu_uart_handle_rx_irq(void) {
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        uint8_t byte = (uint8_t)MINEMU_UART0->rx_data;
        uint32_t next = (rx_head + 1u) % RX_BUFFER_SIZE;
        if (next == rx_tail) {
            /* Lost input invalidates the queued command. */
            rx_tail = rx_head;
            rx_overflow = 1;
        }
        rx_buffer[rx_head] = byte;
        rx_head = next;
    }
}

int minemu_uart_try_getc(void) {
    int result = -1;
    minemu_irq_disable();
    if (rx_overflow) {
        rx_overflow = 0;
        result = -2;
    } else if (rx_tail != rx_head) {
        result = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1u) % RX_BUFFER_SIZE;
    }
    minemu_irq_enable();
    return result;
}
