#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/trap.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

static int32_t uart_ioctl(volatile struct minemu_uart_regs *uart, uint32_t op, uint32_t pointer,
                          uint32_t count) {
    if (op != MINEMU_IOCTL_UART_READ && op != MINEMU_IOCTL_UART_WRITE) {
        return MINEMU_IOCTL_ERROR;
    }
    if ((int32_t)count < 0) {
        return MINEMU_IOCTL_ERROR;
    }
    if (count > MINEMU_IOCTL_MAX_TRANSFER) {
        count = MINEMU_IOCTL_MAX_TRANSFER;
    }
    bool is_read = (op == MINEMU_IOCTL_UART_READ);
    /* Validate the whole range before any visible effect. */
    if (!minemu_user_range_ok(pointer, count, is_read)) {
        return MINEMU_IOCTL_ERROR;
    }

    uint8_t *buffer = (uint8_t *)(uintptr_t)pointer;
    uint32_t done = 0;
    if (is_read) {
        while (done < count && (uart->status & MINEMU_UART_STATUS_RX_READY) != 0) {
            buffer[done++] = (uint8_t)(uart->rx_data & 0xffU);
        }
    } else {
        while (done < count) {
            while ((uart->status & MINEMU_UART_STATUS_TX_READY) == 0) {
            }
            uart->tx_data = buffer[done++];
        }
    }
    return (int32_t)done;
}

static int32_t rng_ioctl(uint32_t op, uint32_t argument) {
    switch (op) {
    case MINEMU_IOCTL_RNG_SEED:
        MINEMU_RNG->seed = argument;
        return 0;
    case MINEMU_IOCTL_RNG_NEXT:
    case MINEMU_IOCTL_RNG_STATE:
        if ((argument & 3U) != 0 || !minemu_user_range_ok(argument, 4U, true)) {
            return MINEMU_IOCTL_ERROR;
        }
        *(uint32_t *)(uintptr_t)argument =
            (op == MINEMU_IOCTL_RNG_NEXT) ? MINEMU_RNG->data : MINEMU_RNG->state;
        return 0;
    default:
        return MINEMU_IOCTL_ERROR;
    }
}

static int32_t do_ioctl(uint32_t fd, uint32_t op, uint32_t argument0, uint32_t argument1) {
    switch (fd) {
    case MINEMU_FD_UART_IN:
    case MINEMU_FD_UART_OUT:
        return uart_ioctl(MINEMU_UART0, op, argument0, argument1);
    case MINEMU_FD_UART_DIAG:
        return uart_ioctl(MINEMU_UART1, op, argument0, argument1);
    case MINEMU_FD_RNG:
        return rng_ioctl(op, argument0);
    case MINEMU_FD_TRACE:
        if (op != MINEMU_IOCTL_TRACE_EVENT) {
            return MINEMU_IOCTL_ERROR;
        }
        MINEMU_TRACE->event = argument0;
        return 0;
    default:
        return MINEMU_IOCTL_ERROR;
    }
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        (frame->spsr & CPSR_MODE_MASK) != CPSR_MODE_USR || (frame->spsr & CPSR_THUMB) != 0 ||
        !minemu_user_range_ok(frame->fault_pc, 4U, false) ||
        *(const uint32_t *)(uintptr_t)frame->fault_pc != A32_SVC_ZERO ||
        frame->r[7] != MINEMU_SYSCALL_IOCTL) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }
    frame->r[0] = (uint32_t)do_ioctl(frame->r[0], frame->r[1], frame->r[2], frame->r[3]);
    return frame;
}