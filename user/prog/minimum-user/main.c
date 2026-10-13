#include <stddef.h>

#include "minemu/user_abi.h"

/* use the updated line limit of 25 bytes */
#define LINE_LIMIT 25u

static void console_putc(char byte) {
    (void)ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)&byte, 1);
}

static void console_puts(const char *text) {
    int length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    (void)ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_WRITE, (void *)text, length);
}

static int console_try_getc(void) {
    unsigned char byte;
    int count = ioctl(MINEMU_FD_UART0, MINEMU_IOCTL_UART_READ, (void *)&byte, 1);
    return count == 1 ? byte : -1;
}

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
        while (*end != '\0') {
            while (*end != '\0' && *end != ' ') {
                console_putc(*end++);
            }
            while (*end == ' ') {
                ++end;
            }
            if (*end != '\0') {
                console_putc(' ');
            }
        }
        console_putc('\n');
        return;
    }

    console_puts("command not found: ");
    while (command < end) {
        console_putc(*command++);
    }
    console_putc('\n');
}

void minemu_user_main(void) {
    char line[LINE_LIMIT + 1];
    size_t length = 0;
    int discarding = 0;

    console_puts("msh> ");
    for (;;) {
        int byte = console_try_getc();
        if (byte == -1) {
            continue;
        }
        if (discarding) {
            if (byte == '\n') {
                discarding = 0;
                console_puts("msh> ");
            }
            continue;
        }
        if (byte == '\n') {
            line[length] = '\0';
            run_command(line);
            length = 0;
            console_puts("msh> ");
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
