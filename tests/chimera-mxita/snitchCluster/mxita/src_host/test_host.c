// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Moritz Scherer <scheremo@iis.ee.ethz.ch>

// Include Standard Libraries

// Include Application Headers
#include "test_cluster.h"
#include "test_host.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "driver.h"

// Include Runtime Headers

// Import HAL Headers

#define STACK_ADDRESS_0 (CLUSTER_0_BASE + 0x20000 - 1)

static offloadArgs_t offloadArgs = {
    .value = 0xdeadbeef,
    .mxita_return = { 0 },
};

int main() {
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

    printf("hello\n");

    #define CHIMERA_RESET_CLUSTER_0_REG_OFFSET 0x20

    volatile uint8_t *regPtr = (volatile uint8_t *)SOC_CTRL_BASE;
    *(regPtr + CHIMERA_RESET_CLUSTER_0_REG_OFFSET) = 0;
    *(regPtr + CHIMERA_CLUSTER_0_CLK_GATE_EN_REG_OFFSET) = 0;

    void *stack_cluster0_ptr[CLUSTER_0_NUMCORES];
    generate_snitchCluster_SPs_uniform(0, (void *)STACK_ADDRESS_0, 0x2000, stack_cluster0_ptr);

    setup_snitchCluster_interruptHandler(clusterInterruptHandler);
    offload_snitchCluster(testReturn, &offloadArgs, stack_cluster0_ptr, 0);

    uint32_t retVal = wait_snitchCluster_return(0);

    for (int i = 0; i < _chimera_numCores[0]; i++) {
        uint32_t core_id = offloadArgs.mxita_return[i].core_id;
        uint32_t is_dm_core = offloadArgs.mxita_return[i].is_dm_core;
        printf("Core ID: %d, is DM core: %d\n", core_id, is_dm_core);
    }

    *(regPtr + CHIMERA_CLUSTER_0_CLK_GATE_EN_REG_OFFSET) = 1;

    // printf("Re: 0x%08x (%d)\n", retVal, retVal);
    // // printf("E: 0x%08x\n", (TESTVAL | 0x000000001));

    // printf("Ret A0: 0x%08x (%d)\n", retVal_0, retVal_0);
    // printf("Ret A1: 0x%08x (%d)\n", retVal_1, retVal_1);
    // printf("Ret A2: 0x%08x (%d)\n", retVal_1, retVal_1);
    // printf("Ret B: 0x%08x (%d)\n", offloadArgs.mxita_return.value, offloadArgs.mxita_return.value);

    volatile int h = 0;
    for (int i = 0; i < 2000; i++) {
        h += i;
    }

    return 0;
}