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

#include "mxita_util.h"

#define MXITA_L 128
#define MXITA_K 32
#include "data.h"

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

SNRT_CLUSTER_L1_ZERO(static volatile int32_t gemm_start_cycle);
SNRT_CLUSTER_L1_ZERO(static volatile int32_t gemm_end_cycle);
SNRT_CLUSTER_L1_ZERO(static volatile int32_t mxita_start_cycle);
SNRT_CLUSTER_L1_ZERO(static volatile int32_t mxita_end_cycle);
SNRT_CLUSTER_L1_ZERO(static volatile int32_t gemm_started);

void sc_st_gemm(gemm_fp_t kernel, sc_st_gemm_args_t *args) {
    // snrt_cluster_hw_barrier();
    // gemm_start_cycle = snrt_mcycle();
    // snrt_cluster_hw_barrier();
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
            snrt_cluster_hw_barrier();
            // START MANTA
            // gemm_start_cycle = snrt_mcycle();
            gemm_started = 1;
            snrt_cluster_hw_barrier();
            
            
            // WAIT FOR MANTA
            // printf("GEMM waiting MANTA\r\n");
            // snrt_cluster_hw_barrier();
            // printf("GEMM starting\r\n");
            // gemm_start_cycle = snrt_mcycle();

            gemm_start_cycle = snrt_mcycle();
            kernel(args->setup_ssr, args->partition_banks, args->transa,
                   args->transb, frac_m, args->n, args->k, a, lda, args->b,
                   args->ldb, args->beta, c, ldc);
            snrt_fpu_fence();
        }
        // snrt_cluster_hw_barrier();
        gemm_end_cycle = snrt_mcycle();
        if (snrt_cluster_core_idx() == 0) {
            float frequency = 40e6;
            uint32_t FLOPs = 2ULL * args->m * args->n * args->k;
            uint32_t MACs = FLOPs / 2;
            uint32_t cucles = gemm_end_cycle - gemm_start_cycle;
            float flops_cycle = (float)FLOPs / cucles;
            float macs_cycle = (float)MACs / cucles;
            float flops_second = flops_cycle * frequency;
            float macs_second = macs_cycle * frequency;
            printf("GEMM completed in %u cycles\r\n", cucles);
            printf("  Total FLOPs: %u\r\n", FLOPs);
            printf("  Total MACs:  %u\r\n", MACs);
            printf("  Performance: %.2f FLOPs/cycle, %.2f MACs/cycle\r\n", flops_cycle, macs_cycle);
            printf("               %.2f GFLOPs/s, %.2f GMACs/s\r\n", flops_second / 1e9, macs_second / 1e9);
            printf(" at %.2f MHz\r\n", frequency / 1e6);
        }
        // snrt_cluster_hw_barrier();
    }
    snrt_cluster_hw_barrier();
}

SNRT_CLUSTER_L1_ZERO(static void *la);
SNRT_CLUSTER_L1_ZERO(static void *lb);
SNRT_CLUSTER_L1_ZERO(static void *lc);

SNRT_CLUSTER_L1_ZERO(static volatile int mxita_core_idx);
SNRT_CLUSTER_L1_ZERO(static void *local_input_matrix_0);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_matrix_0);
SNRT_CLUSTER_L1_ZERO(static void *local_input_scale_0);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_scale_0);
SNRT_CLUSTER_L1_ZERO(static void *local_output_matrix_0);

/**
 * @brief Custom interrupt handler for mxita, which clears the interrupt.
 */
static void hwpeInterruptHandler() { 
    _CLEAR_MSIP();
    snrt_hwpe_clr_mxip(mxita_core_idx);
}

/**
 * @brief L1 allocator allowing custom alignment.
 * Can be used interchangeably with default allocator.
 *
 * @param size Size of the allocation in bytes.
 * @param align Alignment of the allocation in bytes.
 *
 * @returns void* Pointer to the allocated memory.
 */
static inline void *mxita_l1_alloc(size_t size, size_t align) {
    snrt_allocator_t *alloc = snrt_l1_allocator();

#ifdef TRACE_ALLOC
    printf("MXITA L1 ALLOC\r\n");
    printf("  current next:    %#x\r\n", alloc->next);
    printf("  requested size:  %d bytes\r\n", size);
    printf("  requested align: %d bytes\r\n", align);
#endif

    size = ALIGN_UP(size, align);

    size_t pad = ALIGN_UP(alloc->next, align) - alloc->next;
    void *ret = (void *)(alloc->next + pad);
    alloc->next += size + pad;

#ifdef TRACE_ALLOC
    printf("  size: %d bytes\r\n", size);
    printf("  pad:  %d bytes\r\n", pad);
    printf("  ret:  %#x\r\n", ret);
    printf("  next: %#x\r\n", alloc->next);
#endif

    return ret;
}


void mxita_gemm() { // assuming running on the dm core (no need for barriers here)
    uint32_t core_idx = snrt_cluster_core_idx();
    mxita_core_idx = core_idx;

    setup_interruptHandler(hwpeInterruptHandler);
    snrt_interrupt_enable(IRQ_M_ACC);
    hwpe_soft_clear();

    hwpe_set_perfcnt(1);
    hwpe_set_perfcnt_busy(1);

    uint32_t NBYTES_IW_MAT = sizeof(int8_t);
    uint32_t NBYTES_IW_SCALE = sizeof(uint8_t);
    uint32_t NBYTES_OUT_MAT = sizeof(float);

    uint32_t bf16_sel = 1;

    uint8_t lk_size = l_size / k_size;

    uint16_t input_mat_size = N * P * l_size * NBYTES_IW_MAT;
    uint16_t weight_mat_size = M * Q * l_size * NBYTES_IW_MAT;
    uint16_t input_scale_size =
        (N * P * lk_size * NBYTES_IW_SCALE < 512) ? 512 : N * P * lk_size * NBYTES_IW_SCALE;
    uint16_t weight_scale_size =
        (M * Q * lk_size * NBYTES_IW_SCALE < 512) ? 512 : M * Q * lk_size * NBYTES_IW_SCALE;
    uint16_t output_mat_size =
        bf16_sel ? M * N * P * Q * NBYTES_OUT_MAT / 2 : M * N * P * Q * NBYTES_OUT_MAT;

    
    local_input_matrix_0 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
    local_weight_matrix_0 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
    local_input_scale_0 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
    local_weight_scale_0 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
    local_output_matrix_0 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;

    printf("MANTA TCDM buffers:\r\n");
    printf("  local_input_matrix_0  @ %p (size: %d bytes)\r\n", local_input_matrix_0, input_mat_size);
    printf("  local_weight_matrix_0 @ %p (size: %d bytes)\r\n", local_weight_matrix_0, weight_mat_size);
    printf("  local_input_scale_0   @ %p (size: %d bytes)\r\n", local_input_scale_0, input_scale_size);
    printf("  local_weight_scale_0  @ %p (size: %d bytes)\r\n", local_weight_scale_0, weight_scale_size);
    printf("  local_output_matrix_0 @ %p (size: %d bytes)\r\n", local_output_matrix_0, output_mat_size);

    snrt_dma_start_1d(local_input_matrix_0, input_matrix, input_mat_size);
    snrt_dma_start_1d(local_weight_matrix_0, weight_matrix, weight_mat_size);
    snrt_dma_start_1d(local_input_scale_0, input_scale, input_scale_size);
    snrt_dma_start_1d(local_weight_scale_0, weight_scale, weight_scale_size);

    snrt_dma_wait_all();

    hwpe_wait_acquire_job();
    mxita_cfg(
        k_size, l_size, lk_size, 
        (uint32_t)local_input_matrix_0,
        (uint32_t)local_weight_matrix_0, 
        (uint32_t)local_output_matrix_0,
        (uint32_t)local_input_scale_0, 
        (uint32_t)local_weight_scale_0, 
        bf16_sel
    );
    
    snrt_cluster_hw_barrier();
    
    // while (gemm_started == 0) {
    //     // ensure GEMM has started before starting MXITA, to maximize the chance of overlapping them
    // }
    
    // WAIT FOR GEMM
    printf("Waiting for GEMM: %u\r\n", snrt_mcycle());
    snrt_cluster_hw_barrier();
    
    // printf("starting MANTA: %u\r\n", snrt_mcycle());
    // for (volatile int i = 0; i < 10000; i++)
    mxita_start_cycle = snrt_mcycle();
    hwpe_trigger_job();
    snrt_wfi();
    mxita_end_cycle = snrt_mcycle();

    uint32_t hw_cycles = hwpe_get_perfcnt();
    uint32_t bs_cycles = hwpe_get_perfcnt_busy();

    printf("MANTA hw cycles: %u\r\n", hw_cycles);
    printf("MANTA hw busy cycles: %u\r\n", bs_cycles);

    // printf("MANTA end cycle: %u\r\n", snrt_mcycle());
    
    // START GEMM
    // snrt_cluster_hw_barrier();
    // printf("MANTA additional barrier\r\n");

    snrt_cluster_hw_barrier();
}

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

        la = snrt_l1_alloc(size_a) - 0x40000000u + 0x18000000u;
        lb = snrt_l1_alloc(size_b) - 0x40000000u + 0x18000000u;
        lc = snrt_l1_alloc(size_c) - 0x40000000u + 0x18000000u;

        printf("GEMM TCDM buffers:\r\n");
        printf("  largs->a @ %p\r\n", largs->a);
        printf("  largs->b @ %p\r\n", largs->b);
        printf("  largs->c @ %p\r\n", largs->c);
        printf("  la @ %p -> %p (size %u bytes)\r\n", &la, la, size_a);
        printf("  lb @ %p -> %p (size %u bytes)\r\n", &lb, lb, size_b);
        printf("  lc @ %p -> %p (size %u bytes)\r\n", &lc, lc, size_c);

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

    // TODO run mxita here
    
    gemm_started = 0;

    snrt_cluster_hw_barrier();
    
    uint32_t start_cycle = snrt_mcycle();
    snrt_cluster_hw_barrier();
    if (snrt_is_dm_core()) {
        mxita_gemm();

        // snrt_cluster_hw_barrier();
        // snrt_cluster_hw_barrier();
    }
    sc_st_gemm(largs->gemm_fp, &sc_st_args);
    snrt_cluster_hw_barrier();
    uint32_t end_cycle = snrt_mcycle();

    if (snrt_cluster_core_idx() == 0) {
        // mxita lasts much short: ensure the execution was completely overlapped with gemm
        if (mxita_start_cycle < gemm_start_cycle || mxita_end_cycle > gemm_end_cycle) {
            printf("Error: MXITA execution not completely overlapped with GEMM\r\n");
            printf("  GEMM start: %u, end: %u\r\n", gemm_start_cycle, gemm_end_cycle);
            printf("  MXITA start: %u, end: %u\r\n", mxita_start_cycle, mxita_end_cycle);
        } else {
            printf("MXITA execution completely overlapped with GEMM\r\n");
            printf("  GEMM start: %u, end: %u\r\n", gemm_start_cycle, gemm_end_cycle);
            printf("  MXITA start: %u, end: %u\r\n", mxita_start_cycle, mxita_end_cycle);
        }

        // uint32_t FLOPs = 2 * largs->m * largs->n * largs->k;
        // uint32_t MACs = FLOPs / 2;
        // float flops_cycle = (float)FLOPs / (end_cycle - start_cycle);
        // float macs_cycle = (float)MACs / (end_cycle - start_cycle);
        // float flops_second = flops_cycle * argsStruct->frequency;
        // float macs_second = macs_cycle * argsStruct->frequency;

        // printf("GEMM completed in %u cycles\r\n", end_cycle - start_cycle);
        // printf("  Total FLOPs: %u\r\n", FLOPs);
        // printf("  Total MACs:  %u\r\n", MACs);
        // printf("  Performance: %.2f FLOPs/cycle, %.2f MACs/cycle\r\n", flops_cycle, macs_cycle);
        // printf("               %.2f GFLOPs/s, %.2f GMACs/s\r\n", flops_second / 1e9, macs_second / 1e9);
        // printf(" at %.2f MHz\r\n", argsStruct->frequency / 1e6);


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
