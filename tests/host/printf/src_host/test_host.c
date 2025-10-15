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

#ifdef TARGET_PLATFORM_CHIMERA_CONVOLVE
void setGPIO0_UART() {
    // Connect UART port to GPIO 0 Pad
    chimera_padframe_aon_gpio_0_mux_set(CHIMERA_PADFRAME_AON_GPIO_0_group_UART0_port_TX);

    // Set GPIO 0 regs to transmit
    chimera_padframe_aon_gpio_0_cfg_rxe_set(0);  // Disable Pad's Receiver
    chimera_padframe_aon_gpio_0_cfg_trie_set(0); // Disable the tri-state transmitter
}
#endif

int main(void) {
    #ifdef TARGET_PLATFORM_CHIMERA_CONVOLVE
    // Connect UART to GPIO 0
    setGPIO0_UART();
    #endif

    volatile int a = 42;
    printf("Chimera is alive! %d\n", a);

    // JUNGVI: This is here to give enough time to the UART buffer to finish it's transaction
    for (int i = 0; i < 420; i++) {
        a += a * 5;
    }

    return 0;
}
