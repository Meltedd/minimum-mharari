#include <stdint.h>

#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/uart.h"

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    uint32_t source = (uint32_t)frame->exception_id;
    if (source == MINEMU_IRQ_UART0) {
        minemu_uart_handle_rx_irq();
    } else {
        minemu_fail_stop();
    }
    MINEMU_INTERRUPT->eoi = source;
    return frame;
}
