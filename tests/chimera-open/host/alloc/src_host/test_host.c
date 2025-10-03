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

int main(void) {

    int8_t ret = 0;

    int8_t *bufferA = (int8_t *)memory_island_malloc(42);
    int8_t *bufferB = (int8_t *)memory_island_malloc(58);
    memory_island_free(bufferA);
    int8_t *bufferC = (int8_t *)memory_island_malloc(21);

    printf("Buffer A: %p\n", bufferA);
    printf("Buffer B: %p\n", bufferB);
    printf("Buffer C: %p\n", bufferC);

    if (bufferA != bufferC) ret = -1;

    return ret;
}
