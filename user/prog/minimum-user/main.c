#include <stdint.h>

#include "minemu/user_abi.h"

void minemu_user_main(void) {
    static const char message[] = "hello world\n";
    (void)ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE,
                (void *)message, (int)(sizeof(message) - 1U));
    for (;;) {
        __asm__ volatile("nop");
    }
}
