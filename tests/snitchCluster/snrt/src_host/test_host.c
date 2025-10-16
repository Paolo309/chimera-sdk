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

#define STACK_ADDRESS_4 (CLUSTER_4_BASE + 0x20000 - 1)

extern uintptr_t volatile tohost, fromhost;

int main() {

    void *stack_cluster0_ptr[CLUSTER_0_NUMCORES];
    generate_snitchCluster_SPs_uniform(0, (void *)STACK_ADDRESS_4, 0x2000, stack_cluster0_ptr);

    setup_snitchCluster_interruptHandler(clusterInterruptHandler);
    offload_snitchCluster(testReturn, NULL, stack_cluster0_ptr, 4);

    printf_log("Waiting for cluster to finish...\n");

    // Handle tohost/fromhost communication
    while (snitchCluster_busy(4)) {
        // Wait for tohost to be set by the device
        if (tohost != 0) {
            // printf("Host received tohost: %#x\n", tohost);

            // Cluster does tohost = (uintptr_t)buf->hdr.syscall_mem;
            uint32_t *syscall_mem = (uint32_t *)tohost;

            // printf("Host handling syscall %u: fd=%#x, buf=%p, len=%#x\n", syscall_mem[0],
            //        syscall_mem[1], (void *)syscall_mem[2], syscall_mem[3]);
            if (syscall_mem[0] == 64) { // sys_write
                fwrite((const void *)syscall_mem[2], 1, syscall_mem[3], (FILE *)syscall_mem[1]);
                fflush((FILE *)syscall_mem[1]);
            } else {
                printf_log("Unknown syscall: %u\n", syscall_mem[0]);
            }

            fromhost = tohost;
            while (fromhost != 0);

            // printf("Host finished syscall %u\n", syscall_mem[0]);
            tohost = 0;
        }
    }

    uint32_t retVal = wait_snitchCluster_return(4);
    retVal = retVal >> 1;

    printf_log("Returned from cluster: 0x%08x (%d)\n", retVal, retVal);

    return retVal;
}