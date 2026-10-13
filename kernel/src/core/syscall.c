#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/user.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

static int uart_ioctl(volatile struct minemu_uart_regs *uart, uint32_t op,
                      uint32_t address, int32_t length) {
    if (op != MINEMU_IOCTL_UART_READ && op != MINEMU_IOCTL_UART_WRITE) {
        return MINEMU_IOCTL_ERROR;
    }
    if (length < 0) {
        return MINEMU_IOCTL_ERROR;
    }
    uint32_t permissions = op == MINEMU_IOCTL_UART_READ ?
        MINEMU_PTE_WRITABLE : MINEMU_PTE_READABLE;
    if (!minemu_user_range_valid(address, (uint32_t)length, permissions)) {
        return MINEMU_IOCTL_ERROR;
    }

    uint8_t *buffer = (uint8_t *)(uintptr_t)address;
    int count = 0;
    if (op == MINEMU_IOCTL_UART_READ) {
        while (count < length && (uart->status & MINEMU_UART_STATUS_RX_READY) != 0) {
            buffer[count++] = (uint8_t)uart->rx_data;
        }
    } else {
        while (count < length) {
            while ((uart->status & MINEMU_UART_STATUS_TX_READY) == 0) {
            }
            uart->tx_data = buffer[count++];
        }
    }
    return count;
}

static int device_ioctl(uint32_t fd, uint32_t op, uint32_t argument0,
                        uint32_t argument1) {
    switch (fd) {
    case MINEMU_FD_UART0:
        return uart_ioctl(MINEMU_UART0, op, argument0, (int32_t)argument1);
    case MINEMU_FD_UART1:
        return uart_ioctl(MINEMU_UART1, op, argument0, (int32_t)argument1);
    case MINEMU_FD_RNG:
        if (op == MINEMU_IOCTL_RNG_SEED) {
            MINEMU_RNG->seed = argument0;
            return 0;
        }
        if ((op == MINEMU_IOCTL_RNG_NEXT || op == MINEMU_IOCTL_RNG_STATE) &&
            (argument0 & 3U) == 0 &&
            minemu_user_range_valid(argument0, sizeof(uint32_t), MINEMU_PTE_WRITABLE)) {
            uint32_t value = op == MINEMU_IOCTL_RNG_NEXT ?
                MINEMU_RNG->data : MINEMU_RNG->state;
            *(uint32_t *)(uintptr_t)argument0 = value;
            return 0;
        }
        break;
    case MINEMU_FD_TRACE:
        if (op == MINEMU_IOCTL_TRACE_EVENT) {
            MINEMU_TRACE->event = argument0;
            return 0;
        }
        break;
    default:
        break;
    }
    return MINEMU_IOCTL_ERROR;
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    int result = MINEMU_IOCTL_ERROR;
    if (frame->exception_id == MINEMU_EXCEPTION_SVC &&
        (frame->spsr & CPSR_MODE_MASK) == CPSR_MODE_USR &&
        (frame->spsr & CPSR_THUMB) == 0 && (frame->fault_pc & 3U) == 0 &&
        minemu_user_range_valid(frame->fault_pc, sizeof(uint32_t),
                                MINEMU_PTE_READABLE | MINEMU_PTE_EXECUTABLE) &&
        frame->return_lr == frame->fault_pc + 4U &&
        *(const uint32_t *)(uintptr_t)frame->fault_pc == A32_SVC_ZERO &&
        frame->r[7] == MINEMU_SYSCALL_IOCTL) {
        result = device_ioctl(frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
    }
    frame->r[0] = (uint32_t)result;
    return frame;
}
