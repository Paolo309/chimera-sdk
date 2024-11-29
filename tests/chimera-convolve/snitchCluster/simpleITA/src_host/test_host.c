// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Philip Wiese <wiesep@iis.ee.ethz.ch>

#include "test_cluster.h"
#include "test_host.h"

#include "soc.h"
#include "driver.h"
#include "cluster_4.h"

#define STACK_ADDRESS (CLUSTER_4_TCDM_END_ADDR - 8)

static uint32_t *clintPointer = (uint32_t *)CLINT_CTRL_BASE;

static offloadArgs_t offloadArgs = {.value = 0xdeadbeef};

int main() {
    setup_snitchCluster_interruptHandler(clusterInterruptHandler);
    offload_snitchCluster_core(testReturn, &offloadArgs, (void *)(STACK_ADDRESS), 4, 0);
    uint32_t retVal = wait_snitchCluster_return(4);

    return (retVal != (TESTVAL | 0x000000001));
}
