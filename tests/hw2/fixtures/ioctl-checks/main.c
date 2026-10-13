#include <stdint.h>

#include "minemu/user_abi.h"

static const char message[] = "uart0\n";
static uint32_t initialized = UINT32_C(0x12345678);
static uint8_t zeroed[16];

static void check_at(int condition, unsigned int line) {
    if (!condition) {
        (void)ioctl(MINEMU_FD_TRACE, MINEMU_IOCTL_TRACE_EVENT, line);
        __asm__ volatile("bkpt #1");
        for (;;) {
        }
    }
}

#define check(condition) check_at((condition), __LINE__)

static void check_svc(int wrong_instruction) {
    register uint32_t r0 __asm__("r0") = MINEMU_FD_TRACE;
    register uint32_t r1 __asm__("r1") = MINEMU_IOCTL_TRACE_EVENT;
    register uint32_t r2 __asm__("r2") = UINT32_C(0x1234);
    register uint32_t r3 __asm__("r3") = UINT32_C(0x5678);
    register uint32_t r7 __asm__("r7") = wrong_instruction ? MINEMU_SYSCALL_IOCTL : 99;
    if (wrong_instruction) {
        __asm__ volatile("svc #1" : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r7)
                         : : "memory", "cc");
    } else {
        __asm__ volatile("svc #0" : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r7)
                         : : "memory", "cc");
    }
    check((int32_t)r0 == -1 && r1 == MINEMU_IOCTL_TRACE_EVENT &&
          r2 == UINT32_C(0x1234) && r3 == UINT32_C(0x5678) &&
          r7 == (wrong_instruction ? MINEMU_SYSCALL_IOCTL : 99U));
}

static char read_byte(int fd) {
    char byte;
    int count;
    do {
        count = ioctl(fd, MINEMU_IOCTL_UART_READ, (void *)&byte, 1);
    } while (count == 0);
    check(count == 1);
    return byte;
}

void minemu_user_main(void) {
    uint32_t value;
    check(initialized == UINT32_C(0x12345678));
    for (unsigned int index = 0; index < sizeof(zeroed); ++index) {
        check(zeroed[index] == 0);
    }
    check_svc(0);
    check_svc(1);
    check(ioctl(-1, 0) == -1);
    check(ioctl(MINEMU_FD_UART0, 99) == -1);
    check(ioctl(MINEMU_FD_RNG, 99) == -1);
    check(ioctl(MINEMU_FD_TRACE, 99) == -1);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_READ, (void *)0, 0) == 0);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)0, 0) == 0);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)message, -1) == -1);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)UINT32_C(0xfffffff0), 32) == -1);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)UINT32_C(0x007fffff), 2) == -1);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)UINT32_C(0xc0030000), 1) == -1);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)UINT32_C(0x10004000), 1) == -1);

    check(ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_SEED, UINT32_C(0x12345678)) == 0);
    check(ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_NEXT, (unsigned int *)0) == -1);
    check(ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_NEXT,
                (unsigned int *)((uintptr_t)&value + 1U)) == -1);
    check(ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_NEXT,
                (unsigned int *)(uintptr_t)message) == -1);
    check(ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_STATE, &value) == 0);
    check(value == UINT32_C(0x12345678));
    check(ioctl(MINEMU_FD_RNG, MINEMU_IOCTL_RNG_NEXT, &value) == 0);
    check(value == UINT32_C(0x87985aa5));

    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE,
                (void *)message, (int)(sizeof(message) - 1U)) == (int)(sizeof(message) - 1U));
    check(ioctl(MINEMU_FD_UART1, MINEMU_IOCTL_UART_WRITE, (void *)"uart1\n", 6) == 6);
    check(read_byte(MINEMU_FD_UART0) == 'A');
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_READ, (void *)message, 1) == -1);
    check(ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_READ, (void *)UINT32_C(0x007fffff), 2) == -1);
    check(read_byte(MINEMU_FD_UART0) == 'B');
    check(read_byte(MINEMU_FD_UART1) == 'C');
    char buffer[4];
    check(ioctl(MINEMU_FD_UART1, MINEMU_IOCTL_UART_READ, (void *)buffer, 4) == 1);
    check(buffer[0] == 'D');
    check(ioctl(MINEMU_FD_UART1, MINEMU_IOCTL_UART_READ, (void *)buffer, 4) == 0);

    (void)ioctl(MINEMU_FD_TRACE, MINEMU_IOCTL_TRACE_EVENT, UINT32_C(0x49220001));
    __asm__ volatile("bkpt #0");
    for (;;) {
    }
}
