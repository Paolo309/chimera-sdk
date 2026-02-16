// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

// Include Standard Libraries
#include <stdio.h>
#include <string.h>
#include <math.h>

// Include Application Headers
#include "test_cluster.h"
#include "test_host.h"
#include "mxita_util.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

/**
 * @brief Main function of the cluster test.
 *
 * @return int Return 0 if the test was successful, -1 otherwise.
 */
int32_t test_cluster(void *args) {

    /*
     * Initialize the Snitch runtime.
     */
    snrt_init();

    // Clear interrupt from host
    snrt_int_clr_mcip();

    printf("I'm alive!\r\n");

    return 0;
}
