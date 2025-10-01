// Copyright 2025 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Victor Jung <jungvi@iis.ee.ethz.ch>

// Include Standard Libraries
#include <stdio.h>

// Include Target Specific Headers

// Include Driver Headers

// Include Runtime Headers
#include "uart.h"

#ifdef CHIMERA_SIMULATION_BACKEND_RTL
#ifdef CHIMERA_DRIVER_UART
int uart_putc(char c, FILE *file) {
    (void)file;
    default_uart_inst.api->write(&default_uart_inst, &c, 1, NULL);
    return c;
}
#else
int uart_putc(char c, FILE *file) {
    (void)c;
    (void)file;
    return -1;
}
#endif // CHIMERA_DRIVER_UART
#else  // CHIMERA_SIMULATION_BACKEND_GVSOC
int uart_putc(char c, FILE *file) {
    (void)file;
    *(volatile uint32_t *)(long)(0x03004000) = c;
    return c;
}
#endif // CHIMERA_SIMULATION_BACKEND_RTL
