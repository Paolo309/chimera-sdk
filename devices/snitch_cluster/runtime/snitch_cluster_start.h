// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdint.h>

#include "config.h"

extern volatile uint32_t tohost;

#ifndef OPENOCD_SEMIHOSTING
static inline volatile uint32_t *snrt_exit_code_destination() {
    return (volatile uint32_t *)&tohost;
}
#endif

void snrt_init();
void snrt_init_cls();
void snrt_init_bss();
void snrt_printf_init();

#include "start.h"
