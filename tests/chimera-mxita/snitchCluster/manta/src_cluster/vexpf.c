// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Luca Colagrande <colluca@iis.ee.ethz.ch>

#include "math.h"
#include "snrt.h"

#include "vexpf/vexpf.h"

double a[LEN], b_golden[LEN], b_actual[LEN];

int kernel_vexpf() {
    snrt_init();

    uint32_t tstart, tend;

    // Initialize input array
    if (snrt_cluster_core_idx() == 0)
        printf("[cycle=%7u] Starting exponential test from core %d\r\n", snrt_mcycle(), snrt_cluster_core_idx());
        for (int i = 0; i < LEN; i++) a[i] = (float)i / LEN;

    // Calculate exponential of input array using reference implementation
    if (snrt_cluster_core_idx() == 0) {
        printf("[cycle=%7u] Starting golden reference computation from core %d\r\n", snrt_mcycle(), snrt_cluster_core_idx());
        for (int i = 0; i < LEN; i++) {
            b_golden[i] = (double)expf((float)a[i]);
        }
    }

    // Synchronize cores
    snrt_cluster_hw_barrier();

    if (snrt_cluster_core_idx() == 0) {
        printf("[cycle=%7u] Performing vectorized exponential computation... \r\n", snrt_mcycle());
        #if IMPL == IMPL_NAIVE
        printf("[cycle=%7u] Using naive implementation\r\n", snrt_mcycle());
        #elif IMPL == IMPL_BASELINE
        printf("[cycle=%7u] Using baseline implementation\r\n", snrt_mcycle());
        #elif IMPL == IMPL_OPTIMIZED
        printf("[cycle=%7u] Using optimized implementation\r\n", snrt_mcycle());
        #endif
    }

    // Calculate exponential of input array using vectorized implementation
    vexpf_kernel(a, b_actual);

    snrt_cluster_hw_barrier();

    // Check if the results are correct
    if (snrt_cluster_core_idx() == 0) {
        printf("[cycle=%7u] Done.\r\n", snrt_mcycle());
        printf("[cycle=%7u] Starting result comparison from core %d\r\n", snrt_mcycle(), snrt_cluster_core_idx());

        uint32_t n_err = LEN;
        uint32_t tot_err = 0; // TODO remove
        for (int i = 0; i < LEN; i++) {
            if (fabs((float)b_golden[i] - (float)b_actual[i]) > 0.001f) {
                // printf("Error: b_golden[%d] = %f, b_actual[%d] = %f\r\n", i,
                //         (float)b_golden[i], i, (float)b_actual[i]);
                // printf index and fabs diff
                if (tot_err < 5) {
                    printf("Error at index %d: |%f - %f| = %f\r\n", i,
                        (float)b_golden[i], (float)b_actual[i],
                        fabs((float)b_golden[i] - (float)b_actual[i]));
                    tot_err++;
                }
            } else
                n_err--;
        }
        printf("[cycle=%7u] Done. Number of errors: %u / %u (%f\%)\r\n", snrt_mcycle(), n_err, LEN, 100.f * n_err / LEN);
        return n_err;
    } else
        return 0;
}
