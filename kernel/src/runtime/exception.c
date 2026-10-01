#include "minemu/irq.h"
#include "minemu/syscall.h"
#include "minemu/platform.h"

void minemu_fail_stop(void) {
    for (;;) {
        __asm__ volatile("nop");
    }
}

void minemu_panic(const char *message) {
    (void)message;
    minemu_fail_stop();
}

__attribute__((weak, noreturn)) void minemu_svc_trampoline(void) {
    minemu_fail_stop();
}



__attribute__((weak)) struct minemu_trap_frame *minemu_svc_dispatch(
    struct minemu_trap_frame *frame) {
    (void)frame;
    minemu_fail_stop();
}

extern void uart_irq_handler(void);

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    if (frame->exception_id == MINEMU_IRQ_UART0){
        uart_irq_handler();
    }
    MINEMU_INTERRUPT->eoi = frame->exception_id;
    return frame;
}

__attribute__((weak)) void minemu_undefined_dispatch(struct minemu_trap_frame *frame) {
    (void)frame;
    minemu_fail_stop();
}

__attribute__((weak)) void minemu_abort_dispatch(struct minemu_trap_frame *frame) {
    (void)frame;
    minemu_fail_stop();
}
