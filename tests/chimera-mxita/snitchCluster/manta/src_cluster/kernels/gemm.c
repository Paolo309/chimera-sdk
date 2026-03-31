// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

// Include Standard Libraries
#include <stdio.h>
#include <string.h>
#include <math.h>

// Include Application Headers
#include "test_cluster.h"
#include "test_host.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

#include "from_blas.h"
#include "gemm_types.h"
#include "gemm_fp16.h"
#include "gemm_fp32.h"

#define DATA_TYPE_STR(prec) ((prec) == 2 ? "FP16" : (prec == 4 ? "FP32" : "<UNSUPPORTED>"))

// --- CONFIGURATION---

// -- FP16 CONFIG --
#define GEMM_TYPE __fp16
#include "gemm_m16_n32_k128_fp16.h"
static const float ABSOLUTE_TOLERANCE = 1e-2;
static const float RELATIVE_TOLERANCE = 1e-2;

// -- FP32 CONFIG --
// #define GEMM_TYPE float
// #include "gemm_m16_n32_k128_fp32.h"
// static const float ABSOLUTE_TOLERANCE = 1e-3;
// static const float RELATIVE_TOLERANCE = 1e-2;

void sc_st_gemm(gemm_fp_t kernel, sc_st_gemm_args_t *args) {
    if (snrt_is_compute_core()) {
        const uint32_t core_num = snrt_cluster_compute_core_num();
        const uint32_t core_idx = snrt_cluster_core_idx();

        // Compute cores work not on contiguous blocks but on strided rows
        uint32_t lda = core_num * args->lda;
        uint32_t ldc = core_num * args->ldc;

        // Compute cores access A and C at offsets of one row from each other
        uint32_t offset_a = core_idx * args->lda * args->prec;
        uint32_t offset_c = core_idx * args->ldc * args->prec;
        void *a = (void *)((uintptr_t)(args->a) + offset_a);
        void *c = (void *)((uintptr_t)(args->c) + offset_c);

        // Compute fraction of C rows every core computes
        uint32_t frac_m = args->m / core_num;
        uint32_t rem_m = args->m % core_num;
        if (snrt_cluster_core_idx() < rem_m) frac_m++;

        // Invoke kernel for each core
        if (frac_m > 0) {
            kernel(args->setup_ssr, args->partition_banks, args->transa,
                   args->transb, frac_m, args->n, args->k, a, lda, args->b,
                   args->ldb, args->beta, c, ldc);
            snrt_fpu_fence();
        }
    }
}

SNRT_CLUSTER_L1_ZERO(static void *la);
SNRT_CLUSTER_L1_ZERO(static void *lb);
SNRT_CLUSTER_L1_ZERO(static void *lc);

int32_t kernel_gemm(void *args) {
    /*
     * Initialize the Snitch runtime.
     */
    snrt_init();

    snrt_int_clr_mcip();

    offloadArgs_t *argsStruct = (offloadArgs_t *)args;
    const gemm_args_t *largs = &gemm_args;

    snrt_cluster_hw_barrier();

    const size_t size_a = largs->m * largs->k * largs->prec;
    const size_t size_b = largs->k * largs->n * largs->prec;
    const size_t size_c = largs->m * largs->n * largs->prec;

    if (snrt_is_dm_core()) {
        printf("BLAS GEMM kernel test\r\n");
        printf("+----------------------------\r\n");
        printf("| (M, N, K) = (%d, %d, %d)\r\n", largs->m, largs->n, largs->k);
        printf("| data type =  %s\r\n", DATA_TYPE_STR(largs->prec));
        printf("| (%dx%d)*(%dx%d)=(%dx%d)\r\n", largs->m, largs->k, largs->k, largs->n, largs->m, largs->n);
        printf("+----------------------------\r\n");

        la = snrt_l1_alloc(size_a);
        lb = snrt_l1_alloc(size_b);
        lc = snrt_l1_alloc(size_c);

        printf("largs->a @ %p\r\n", largs->a);
        printf("largs->b @ %p\r\n", largs->b);
        printf("largs->c @ %p\r\n", largs->c);
        printf("la @ %p -> %p (size %u bytes)\r\n", &la, la, size_a);
        printf("lb @ %p -> %p (size %u bytes)\r\n", &lb, lb, size_b);
        printf("lc @ %p -> %p (size %u bytes)\r\n", &lc, lc, size_c);

        snrt_dma_start_1d(la, largs->a, size_a);
        snrt_dma_start_1d(lb, largs->b, size_b);
        snrt_dma_start_1d(lc, largs->c, size_c);

        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();

    uint32_t tile_m = largs->m / largs->m_tiles; // XXX assuming 1 tile
    uint32_t tile_n = largs->n / largs->n_tiles; // XXX assuming 1 tile
    uint32_t tile_k = largs->k / largs->k_tiles; // XXX assuming 1 tile

    const double beta_k = (double)largs->beta; // XXX assuming 0

    sc_st_gemm_args_t sc_st_args;
    sc_st_args.prec = largs->prec;
    sc_st_args.setup_ssr = largs->setup_ssr;
    sc_st_args.partition_banks = largs->partition_banks;
    sc_st_args.transa = largs->transa;
    sc_st_args.transb = largs->transb;
    sc_st_args.a = la;
    if (largs->transa) {
        sc_st_args.lda = tile_m;
    } else if (largs->partition_banks) {
        printf("NOT SUPPORTED: partition_banks with transa\r\n");
    } else {
        sc_st_args.lda = tile_k;
    }
    sc_st_args.b = lb;
    if (largs->transb) {
        sc_st_args.ldb = tile_k;
    } else if (largs->partition_banks) {
        printf("NOT SUPPORTED: partition_banks with transb\r\n");
    } else {
        sc_st_args.ldb = tile_n;
    }
    sc_st_args.beta = beta_k;
    sc_st_args.c = lc;
    if (largs->partition_banks) {
        printf("NOT SUPPORTED: partition_banks with c\r\n");
    } else {
        sc_st_args.ldc = tile_n;
    }
    sc_st_args.m = tile_m;
    sc_st_args.n = tile_n;
    sc_st_args.k = tile_k;
    
    snrt_cluster_hw_barrier();
    uint32_t start_cycle = snrt_mcycle();
    snrt_cluster_hw_barrier();
    sc_st_gemm(largs->gemm_fp, &sc_st_args);
    snrt_cluster_hw_barrier();
    uint32_t end_cycle = snrt_mcycle();    

    if (snrt_cluster_core_idx() == 0) {
        uint32_t FLOPs = 2 * largs->m * largs->n * largs->k;
        uint32_t MACs = FLOPs / 2;
        float flops_cycle = (float)FLOPs / (end_cycle - start_cycle);
        float macs_cycle = (float)MACs / (end_cycle - start_cycle);
        float flops_second = flops_cycle * argsStruct->frequency;
        float macs_second = macs_cycle * argsStruct->frequency;

        printf("GEMM completed in %u cycles\r\n", end_cycle - start_cycle);
        printf("  Total FLOPs: %u\r\n", FLOPs);
        printf("  Total MACs:  %u\r\n", MACs);
        printf("  Performance: %.2f FLOPs/cycle, %.2f MACs/cycle\r\n", flops_cycle, macs_cycle);
        printf("               %.2f GFLOPs/s, %.2f GMACs/s\r\n", flops_second / 1e9, macs_second / 1e9);
        printf(" at %.2f MHz\r\n", argsStruct->frequency / 1e6);


        printf("--- Starting DUT vs REF comparison --- \r\n");

        int total_comparisons = size_c / largs->prec;

        // for RTL, we just compare a few values
        if (argsStruct->is_rtl) {
            total_comparisons = 5;
        }

        printf("Performing %d comparisons...\r\n", total_comparisons);

        int errors = 0;
        GEMM_TYPE *actual = (GEMM_TYPE *)lc;
        GEMM_TYPE *golden = (GEMM_TYPE *)result;
        for (int i = 0; i < total_comparisons; i++) {
            float dut = (float)actual[i];
            float ref = (float)golden[i];
            float abs_err = fabsf(dut - ref);
            float max_err = ABSOLUTE_TOLERANCE + RELATIVE_TOLERANCE * fabsf(ref);
            if (abs_err > max_err) {
                errors += 1;
                printf("DUT OUT VS REF OUT [%d]: %f vs %f\r\n", i, dut, ref);
            }
        }
        printf("Number of errors: %d over %d, %.2f%%\r\n", errors, total_comparisons,
               100.f * errors / total_comparisons);
        
        return errors << 1;
    }

    return 0;
}
