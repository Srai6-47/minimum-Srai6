#ifndef MINEMU_SYSCALL_H
#define MINEMU_SYSCALL_H

#include <stdbool.h>
#include <stdint.h>

#include "minemu/trap.h"
#include "minemu/user_abi.h"

/* True if [vaddr, vaddr + length) is entirely mapped user memory (writable if write). */
bool minemu_user_range_ok(uint32_t vaddr, uint32_t length, bool write);

void minemu_svc_trampoline(void) __attribute__((noreturn));
struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame);

#endif