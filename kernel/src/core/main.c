#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/irq.h"
#include "minemu/platform.h"

volatile char rx_buffer[21];
volatile int rx_index = 0;
volatile int line_ready = 0;

void uart_irq_handler(void) {
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) {
        char c = (char)MINEMU_UART0->rx_data;

        if (c == '\n') {
            rx_buffer[rx_index] = '\0';
            line_ready = 1;
        } else if (c == 0x08 || c == 0x7f) {
            if (rx_index > 0) {
                rx_index--;
            }
        } else if (rx_index < 20) {
            rx_buffer[rx_index++] = c;
        }
    }
}

void uart_c(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
        // Wait until the UART is ready to transmit
    }
        MINEMU_UART0->tx_data = (uint32_t)c;
}

void uart_str(const char *str) {
    while (*str != '\0') {
        uart_c(*str);
        str++;
    }
}

void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }
    // 1. Enable UART RX interrupt generation (Bit 0 of control register)
    MINEMU_UART0->control |= 1;

    // 2. Unmask UART0 on the interrupt controller
    MINEMU_INTERRUPT->enable |= (UINT32_C(1) << MINEMU_IRQ_UART0);

    // 3. Enable CPU interrupts globally
    minemu_irq_enable();

    // 4. Start msh loop
    while (1) {
        uart_str("msh> ");

        // Wait until the interrupt handler sets line_ready
        while (!line_ready) {
            // wait
        }

        // Disable interrupts while reading shared state to prevent race conditions
        minemu_irq_disable();
        
        // --- Process the command ---
        int i = 0;
        // Ignore leading spaces
        while (rx_buffer[i] == ' ') i++;

        if (rx_buffer[i] != '\0') { // If not an empty line
            // Check for "echo" command
            if (rx_buffer[i]=='e' && rx_buffer[i+1]=='c' && rx_buffer[i+2]=='h' && rx_buffer[i+3]=='o') {
                i += 4;
                // Ignore spaces after echo
                while (rx_buffer[i] == ' ') i++;
                
                // Print the rest of the arguments
                if (rx_buffer[i] != '\0') {
                    uart_str((const char*)&rx_buffer[i]);
                }
                uart_c('\n');
            } else {
                // Command not found
                uart_str("command not found: ");
                
                // Print just the first word
                while (rx_buffer[i] != ' ' && rx_buffer[i] != '\0') {
                    uart_c(rx_buffer[i]);
                    i++;
                }
                uart_c('\n');
            }
        }

        // Reset buffer for next line
        rx_index = 0;
        line_ready = 0;
        
        // Re-enable interrupts
        minemu_irq_enable();
    }
}
