// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

#ifndef _TEST_CLUSTER_INCLUDE_GUARD_
#define _TEST_CLUSTER_INCLUDE_GUARD_

#include <stdint.h>

/**
 * @brief Interrupt handler for the cluster, which clears the interrupt flag for the current hart.
 *
 * @warning Stack, thread and global pointer might not yet be set up!
 */
void clusterInterruptHandler();

// MXITA tests

int32_t mxita_test_default(void *args);
int32_t benchmark_frep(void *args);
int32_t benchmark_freb4d_4(void *args);
int32_t benchmark_gemm_frep(void *args);
int32_t test_cluster(void *args);
int32_t benchmark_dma_bw(void *args);
int32_t kernel_gemm(void *args);

#endif //_TEST_CLUSTER_INCLUDE_GUARD_
