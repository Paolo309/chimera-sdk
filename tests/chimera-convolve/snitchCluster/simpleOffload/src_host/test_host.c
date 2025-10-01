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

#define STACK_ADDRESS (CLUSTER_4_BASE + 0x20000 - 8)

static offloadArgs_t offloadArgs = {.value = 0xdeadbeef};

int main() {
    setup_snitchCluster_interruptHandler(clusterInterruptHandler);
    offload_snitchCluster_core(testReturn, &offloadArgs, (void *)(STACK_ADDRESS), 4, 0);
    uint32_t retVal = wait_snitchCluster_return(4);

    printf("Returned value: 0x%08x\n", retVal);
    printf("Expected value: 0x%08x\n", (TESTVAL | 0x000000001));

    return (retVal != (TESTVAL | 0x000000001));
}