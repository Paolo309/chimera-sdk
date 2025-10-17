// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Victor Jung <jungvi@iis.ee.ethz.ch>

// Include Standard Libraries
#include <stdio.h>
#include <string.h>

// Include Application Headers

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "driver.h"

// Include Runtime Headers
#include "util.h"

// Import HAL Headers
#include "clint.h"
#include "uart.h"
#include "interface_api.h"

int main(void) {

    // 1. Configure the UART from defaults
    uart_config_t uart_cfg = default_uart_cfg;

    // 2. Read the RTC frequency from a hardware register
    uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);

    // 3. Calculate the desired core frequency from the RTC frequency
    uint32_t reset_freq = clint_get_core_freq(rtc_freq, 2500);

    // 4. Update the UART config with the calculated frequency, and proper BAUD rate
    uart_cfg.clk_freq_hz = reset_freq;
    uart_cfg.baud_rate = 115200;

    // 5. Initialize the UART interface
    chi_interface_t uart_iface = {
        .base = (uintptr_t)&__base_uart,
        .cfg = &uart_cfg,
        .api = &default_uart_api,
    };

    // 6. Open the UART interface
    if (iface_open(&uart_iface) != 0) {
        return -1;
    }

    volatile int a = 42;
    printf("Chimera is alive! %d\n", a);

    // JUNGVI: This is here to give enough time to the UART buffer to finish it's transaction
    for (int i = 0; i < 420; i++) {
        a += a * 5;
    }

    return 0;
}
