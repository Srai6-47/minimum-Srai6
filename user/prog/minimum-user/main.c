#include <stdbool.h>
#include <stdint.h>

#include "minemu/user_abi.h"

#define LINE_MAX_BYTES 64U

static void write_all(const char *text, uint32_t length) {
    while (length > 0) {
        int32_t written = ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE, (void *)text, length);
        if (written <= 0) {
            return;
        }
        text += written;
        length -= (uint32_t)written;
    }
}

static void write_str(const char *text) {
    uint32_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    write_all(text, length);
}

static void run_command(const char *line, uint32_t length) {
    if (length == 0) {
        return;
    }
    uint32_t word = 0;
    while (word < length && line[word] != ' ') {
        ++word;
    }
    if (word == 4U && line[0] == 'e' && line[1] == 'c' && line[2] == 'h' && line[3] == 'o') {
        if (length > 5U) {
            write_all(line + 5, length - 5U);
        }
        write_all("\n", 1U);
    } else {
        write_str("command not found: ");
        write_all(line, word);
        write_all("\n", 1U);
    }
}

void minemu_user_main(void) {
    char line[LINE_MAX_BYTES + 1U];
    uint32_t length = 0;
    bool overflow = false;

    write_str("msh> ");
    for (;;) {
        char c = 0;
        if (ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ, (void *)&c, 1U) != 1) {
            continue;
        }
        if (c == '\n') {
            if (!overflow) {
                while (length > 0 && line[length - 1U] == ' ') {
                    --length;
                }
                line[length] = '\0';
                run_command(line, length);
            }
            length = 0;
            overflow = false;
            write_str("msh> ");
        } else if (c == 0x08 || c == 0x7f) {
            if (!overflow && length > 0) {
                --length;
            }
        } else if (!overflow) {
            if (c == ' ' && (length == 0 || line[length - 1U] == ' ')) {
                continue;
            }
            if (length == LINE_MAX_BYTES) {
                overflow = true;
            } else {
                line[length++] = c;
            }
        }
    }
}