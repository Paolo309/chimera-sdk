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

static offloadArgs_t offloadArgs = {.value = 0xdeadbeef};

int main() {
    void *stack_cluster0_ptr[CLUSTER_0_NUMCORES];
    generate_snitchCluster_SPs_uniform(0, (void *)STACK_ADDRESS_0, 0x2000, stack_cluster0_ptr);

    setup_snitchCluster_interruptHandler(clusterInterruptHandler);
    offload_snitchCluster(testReturn, &offloadArgs, stack_cluster0_ptr, 0);
    uint32_t retVal = wait_snitchCluster_return(0);

    printf("Returned value: 0x%08x (%d)\n", retVal, retVal);
    printf("Expected value: 0x%08x\n", (TESTVAL | 0x000000001));

    return (retVal != (TESTVAL | 0x000000001));
}