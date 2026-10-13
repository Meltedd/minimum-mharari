#ifndef MINEMU_USER_H
#define MINEMU_USER_H

#include <stdbool.h>
#include <stdint.h>

#include "minemu/boot.h"

void minemu_start_user(const struct minemu_boot_info *boot_info) __attribute__((noreturn));
bool minemu_user_range_valid(uint32_t address, uint32_t length, uint32_t permissions);

#endif
