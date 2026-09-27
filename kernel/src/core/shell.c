#include <stddef.h>

#include "minemu/shell.h"
#include "minemu/uart.h"

#define LINE_LIMIT 20u

static void run_command(char *line) {
    char *command = line;
    while (*command == ' ') {
        ++command;
    }
    if (*command == '\0') {
        return;
    }

    char *end = command;
    while (*end != '\0' && *end != ' ') {
        ++end;
    }
    if (end - command == 4 && command[0] == 'e' && command[1] == 'c' &&
        command[2] == 'h' && command[3] == 'o') {
        while (*end == ' ') {
            ++end;
        }
        minemu_uart_puts(end);
        minemu_uart_putc('\n');
        return;
    }

    minemu_uart_puts("command not found: ");
    while (command < end) {
        minemu_uart_putc(*command++);
    }
    minemu_uart_putc('\n');
}

void minemu_shell_run(void) {
    char line[LINE_LIMIT + 1];
    size_t length = 0;
    int discarding = 0;

    minemu_uart_puts("msh> ");
    for (;;) {
        int byte = minemu_uart_try_getc();
        if (byte == -1) {
            continue;
        }
        if (byte == -2) {
            length = 0;
            discarding = 1;
            continue;
        }
        if (discarding) {
            if (byte == '\n') {
                discarding = 0;
                minemu_uart_puts("msh> ");
            }
            continue;
        }
        if (byte == '\n') {
            line[length] = '\0';
            run_command(line);
            length = 0;
            minemu_uart_puts("msh> ");
        } else if (byte == 0x08 || byte == 0x7f) {
            if (length != 0) {
                --length;
            }
        } else if (length < LINE_LIMIT) {
            line[length++] = (char)byte;
        } else {
            length = 0;
            discarding = 1;
        }
    }
}
