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

// #define STACK_ADDRESS_0 (CLUSTER_0_BASE + 0x20000 - 1)
// #define STACK_ADDRESS_4 (CLUSTER_4_BASE + 0x20000 - 1)

#define CLUSTER 4
#define STACK_ADDRESS (_chimera_clusterBase[CLUSTER] + 0x20000 - 1)

extern uintptr_t volatile tohost, fromhost;

static offloadArgs_t offloadArgs = {0};

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

    printf("offloading . . .\n");
    offload_snitchCluster(testReturn, &offloadArgs, stack_cluster_ptr, CLUSTER);

    printf("Waiting for cluster to finish...\n");

    // Handle tohost/fromhost communication
    while (snitchCluster_busy(CLUSTER)) {
        // Wait for tohost to be set by the device
        if (tohost != 0) {
            volatile uint32_t syscall_addr = tohost;

            // Acknowledge tohost
            tohost = 0;

            // printf("Host received tohost: %#x\n", tohost);

            // Cluster does tohost = (uintptr_t)buf->hdr.syscall_mem;
            uint32_t *syscall_mem = (uint32_t *)syscall_addr;

            // printf("Host handling syscall %u: fd=%#x, buf=%p, len=%#x\n", syscall_mem[0],
            //        syscall_mem[1], (void *)syscall_mem[2], syscall_mem[3]);
            if (syscall_mem[0] == 64) { // sys_write
                fwrite((const void *)syscall_mem[2], 1, syscall_mem[3], (FILE *)syscall_mem[1]);
                fflush((FILE *)syscall_mem[1]);
            } else {
                printf_log("Unknown syscall: %u\n", syscall_mem[0]);
            }

            // Notify cluster that syscall is done
            fromhost = syscall_addr;
        }
    }

    uint32_t retVal = wait_snitchCluster_return(CLUSTER);
    retVal = retVal >> 1;

    // printf("result_ptr from cluster: %p\n", offloadArgs.result_ptr);

    // for (int i = 0; i < 10; i++) {
    //     printf("Cluster result[%d]: %d\n", i, offloadArgs.result_ptr[i]);
    // }

    // unsigned int n = 0x4229AE14;
    // float f = *(float *)&n;
    // float f = 42.42f;
    // printf("conversion to float: %f\n", f); // expected: 42.42

    set_snitchCluster_clockGating(CLUSTER, 1);
    set_snitchCluster_reset(CLUSTER, 1);

    // printf("done\n");
    // printf_log("Cluster result[33]: %d\n", offloadArgs.result[33]);
    printf_log("Cluster returned: %d\n", retVal);
    printf_log("Cycles taken by core 2: %u\n", offloadArgs.cycles);

    return retVal;
}
