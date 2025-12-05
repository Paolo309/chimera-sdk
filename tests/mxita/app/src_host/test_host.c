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
#include "log.h"

// Import HAL Headers
// #include "interface_api.h"

#define CLUSTER 0
#define STACK_ADDRESS (_chimera_clusterBase[CLUSTER] + 0x20000 - 1)

// Cluster syscall communication
extern uintptr_t volatile tohost, fromhost;
void handle_cluster_syscalls(int cluster_id);

static offloadArgs_t offloadArgs = {0};

uint32_t mxita_default_test(void *stack_cluster_ptr) {
    set_snitchCluster_clockGating(CLUSTER, 0);
    offload_snitchCluster(testReturn, &offloadArgs, stack_cluster_ptr, CLUSTER);

    // Handle tohost/fromhost communication, returns when cluster is done
    handle_cluster_syscalls(CLUSTER);

    uint32_t retVal = wait_snitchCluster_return(CLUSTER);
    set_snitchCluster_clockGating(CLUSTER, 1);

    return retVal >> 1;
}

int main() {
    // void *stack_cluster_ptr[CLUSTER_0_NUMCORES];
    void *stack_cluster_ptr[NUM_CLUSTER_CORES];
    generate_snitchCluster_SPs_uniform(0, (void *)STACK_ADDRESS, 0x2000, stack_cluster_ptr);
    setup_snitchCluster_interruptHandler(clusterInterruptHandler);

    // set_snitchCluster_reset(CLUSTER, 0);
    set_snitchCluster_clockGating(CLUSTER, 0);

    set_snitchCluster_reset(CLUSTER, 1);
    for (volatile int i = 0; i < 10; i++);
    set_snitchCluster_reset(CLUSTER, 0);

#if defined(HARDWARE_BACKEND_RTL)
    offloadArgs.is_rtl = 1;
#endif

    printf("=== MXITA Test @ " BACKEND_NAME " ===\r\n");

    uint32_t retVal;
    uint32_t failed_tests = 0;

    printf_log(" - Test 0 | default | BF32\r\n");
    offloadArgs.bf16_sel = 0; // BF32
    retVal = mxita_default_test(stack_cluster_ptr);
    printf_log("[%s] Cluster returned: %d\r\n\r\n", retVal == 0 ? "PASS" : "FAIL", retVal);
    failed_tests += (retVal != 0);

    printf_log(" - Test 1 | default | BF16\r\n");
    offloadArgs.bf16_sel = 1; // BF16
    retVal = mxita_default_test(stack_cluster_ptr);
    printf_log("[%s] Cluster returned: %d\r\n\r\n", retVal == 0 ? "PASS" : "FAIL", retVal);
    failed_tests += (retVal != 0);

    return failed_tests;
}


void handle_cluster_syscalls(int cluster_id) {
    while (snitchCluster_busy(cluster_id)) {
        // Wait for tohost to be set by the device
        if (tohost != 0) {
            volatile uint32_t syscall_addr = tohost;

            // Acknowledge tohost
            tohost = 0;

            // printf("Host received tohost: %#x\r\n", tohost);

            // Cluster does tohost = (uintptr_t)buf->hdr.syscall_mem;
            uint32_t *syscall_mem = (uint32_t *)syscall_addr;

            // printf("Host handling syscall %u: fd=%#x, buf=%p, len=%#x\r\n", syscall_mem[0],
            //        syscall_mem[1], (void *)syscall_mem[2], syscall_mem[3]);
            if (syscall_mem[0] == 64) { // sys_write
                fwrite((const void *)syscall_mem[2], 1, syscall_mem[3], (FILE *)syscall_mem[1]);
                fflush((FILE *)syscall_mem[1]);
                // printf_log("handled syscall: %u\r\n", syscall_mem[0]);
            } else {
                printf_log("Unknown syscall: %u\r\n", syscall_mem[0]);
            }

            // Notify cluster that syscall is done
            fromhost = syscall_addr;
        }
    }
}
