#ifndef MINEMU_UART_H
#define MINEMU_UART_H

void minemu_uart_putc(char byte);
void minemu_uart_puts(const char *text);
void minemu_uart_enable_rx(void);
void minemu_uart_handle_rx_irq(void);
/* Returns a byte, -1 when empty, or -2 after input was lost. */
int minemu_uart_try_getc(void);

#endif
