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

#define MXITA_L 64
#define MXITA_K 32
#include "data.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

#define TRACE
// #define TRACE_ALLOC

#define NUM_CONTEXTS 1

SNRT_CLUSTER_L1_ZERO(static int mxita_test_failed);

SNRT_CLUSTER_L1_ZERO(static void *local_input_matrix_0);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_matrix_0);
SNRT_CLUSTER_L1_ZERO(static void *local_input_scale_0);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_scale_0);
SNRT_CLUSTER_L1_ZERO(static void *local_output_matrix_0);

#if NUM_CONTEXTS > 1
SNRT_CLUSTER_L1_ZERO(static void *local_input_matrix_1);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_matrix_1);
SNRT_CLUSTER_L1_ZERO(static void *local_input_scale_1);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_scale_1);
SNRT_CLUSTER_L1_ZERO(static void *local_output_matrix_1);
#endif

#if NUM_CONTEXTS > 2
SNRT_CLUSTER_L1_ZERO(static void *local_input_matrix_2);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_matrix_2);
SNRT_CLUSTER_L1_ZERO(static void *local_input_scale_2);
SNRT_CLUSTER_L1_ZERO(static void *local_weight_scale_2);
SNRT_CLUSTER_L1_ZERO(static void *local_output_matrix_2);
#endif

SNRT_CLUSTER_L1_ZERO (static uint32_t hw_cycles);
SNRT_CLUSTER_L1_ZERO (static uint32_t sw_cycles);

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

// --------------------------------------------------------------------------

SNRT_CLUSTER_L1_ZERO(static volatile int mxita_core_idx);
#if NUM_CONTEXTS > 1
SNRT_CLUSTER_L1_ZERO(static int mxita_completed_runs);
#endif

/**
 * @brief Custom interrupt handler for mxita, which clears the interrupt.
 */
static void hwpeInterruptHandler() { 
    _CLEAR_MSIP();

    snrt_hwpe_clr_mxip(mxita_core_idx);
#if NUM_CONTEXTS > 1
    ++mxita_completed_runs;
#endif
}

/**
 * @brief Main function of the cluster test.
 *
 * @return int Return 0 if the test was successful, -1 otherwise.
 */
int32_t mxita_test_default(void *args) {

    /*
     * Initialize the Snitch runtime.
     */
    snrt_init();

    uint32_t core_idx = snrt_cluster_core_idx();

    // Clear interrupt from host
    snrt_int_clr_mcip();

    // Setup custom interrupt handler for the cluster cores
    setup_interruptHandler(hwpeInterruptHandler);

    // Enable accelerator interrupts
    snrt_interrupt_enable(IRQ_M_ACC);

    if (core_idx == 0) {
        printf("Default test\r\n");
        printf("Running MXITA on cluster %d with %d cores\r\n", snrt_cluster_idx(),
               _chimera_numCores[snrt_cluster_idx()]);
    }
    snrt_cluster_hw_barrier();

    offloadArgs_t *argsStruct = (offloadArgs_t *)args;

    uint32_t NBYTES_IW_MAT = sizeof(int8_t);
    uint32_t NBYTES_IW_SCALE = sizeof(uint8_t);
    uint32_t NBYTES_OUT_MAT = sizeof(float);

    uint32_t bf16_sel = argsStruct->bf16_sel;

    uint8_t lk_size = l_size / k_size;

    uint16_t input_mat_size = N * P * l_size * NBYTES_IW_MAT;
    uint16_t weight_mat_size = M * Q * l_size * NBYTES_IW_MAT;
    uint16_t input_scale_size =
        (N * P * lk_size * NBYTES_IW_SCALE < 512) ? 512 : N * P * lk_size * NBYTES_IW_SCALE;
    uint16_t weight_scale_size =
        (M * Q * lk_size * NBYTES_IW_SCALE < 512) ? 512 : M * Q * lk_size * NBYTES_IW_SCALE;
    uint16_t output_mat_size =
        bf16_sel ? M * N * P * Q * NBYTES_OUT_MAT / 2 : M * N * P * Q * NBYTES_OUT_MAT;

    if (snrt_is_dm_core()) {
        printf("+----------------------------\r\n");
        printf("| (M, N, P, Q) = (%d, %d, %d, %d)\r\n", M, N, P, Q);
        printf("| (L, K, LK)   = (%d, %d, %d)\r\n", l_size, k_size, lk_size);
        printf("| output type  =  %s\r\n", bf16_sel ? "BF16" : "FP32");
        printf("| (%dx%d)*(%dx%d)=(%dx%d)\r\n", N*P, l_size, l_size, M*Q, N*P, M*Q);
        printf("+----------------------------\r\n");

#ifdef TRACE
        snrt_allocator_t *alloc = snrt_l1_allocator();
        uint32_t mxita_tcdm_start = alloc->next;
#endif

        local_input_matrix_0 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN);
        local_weight_matrix_0 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN);
        local_input_scale_0 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN);
        local_weight_scale_0 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN);
        local_output_matrix_0 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN);

#if NUM_CONTEXTS > 1
        local_input_matrix_1 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN);
        local_weight_matrix_1 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN);
        local_input_scale_1 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN);
        local_weight_scale_1 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN);
        local_output_matrix_1 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN);
#endif

#if NUM_CONTEXTS > 2
        local_input_matrix_2 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN);
        local_weight_matrix_2 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN);
        local_input_scale_2 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN);
        local_weight_scale_2 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN);
        local_output_matrix_2 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN);
#endif

#ifdef TRACE
        uint32_t mxita_tcdm_end = alloc->next;

        printf("MANTA TCDM buffers:\r\n");
        printf("  local_input_matrix_0  @ %p (size: %d bytes)\r\n", local_input_matrix_0, input_mat_size);
        printf("  local_weight_matrix_0 @ %p (size: %d bytes)\r\n", local_weight_matrix_0, weight_mat_size);
        printf("  local_input_scale_0   @ %p (size: %d bytes)\r\n", local_input_scale_0, input_scale_size);
        printf("  local_weight_scale_0  @ %p (size: %d bytes)\r\n", local_weight_scale_0, weight_scale_size);
        printf("  local_output_matrix_0 @ %p (size: %d bytes)\r\n", local_output_matrix_0, output_mat_size);
#if NUM_CONTEXTS > 1
        printf("  local_input_matrix_1  @ %p (size: %d bytes)\r\n", local_input_matrix_1, input_mat_size);
        printf("  local_weight_matrix_1 @ %p (size: %d bytes)\r\n", local_weight_matrix_1, weight_mat_size);
        printf("  local_input_scale_1   @ %p (size: %d bytes)\r\n", local_input_scale_1, input_scale_size);
        printf("  local_weight_scale_1  @ %p (size: %d bytes)\r\n", local_weight_scale_1, weight_scale_size);
        printf("  local_output_matrix_1 @ %p (size: %d bytes)\r\n", local_output_matrix_1, output_mat_size);
#endif
#if NUM_CONTEXTS > 2
        printf("  local_input_matrix_2  @ %p (size: %d bytes)\r\n", local_input_matrix_2, input_mat_size);
        printf("  local_weight_matrix_2 @ %p (size: %d bytes)\r\n", local_weight_matrix_2, weight_mat_size);
        printf("  local_input_scale_2   @ %p (size: %d bytes)\r\n", local_input_scale_2, input_scale_size);
        printf("  local_weight_scale_2  @ %p (size: %d bytes)\r\n", local_weight_scale_2, weight_scale_size);
        printf("  local_output_matrix_2 @ %p (size: %d bytes)\r\n", local_output_matrix_2, output_mat_size);
#endif

        uint32_t total_mxita_tcdm_usage = mxita_tcdm_end - mxita_tcdm_start;
        printf("Total MXITA TCDM usage: %.2f KiB (%.2f%% of total TCDM)\r\n", total_mxita_tcdm_usage / 1024.f,
               100.f * total_mxita_tcdm_usage / SNRT_TCDM_SIZE);
#endif

        snrt_dma_start_1d(local_input_matrix_0, input_matrix, input_mat_size);
        snrt_dma_start_1d(local_weight_matrix_0, weight_matrix, weight_mat_size);
        snrt_dma_start_1d(local_input_scale_0, input_scale, input_scale_size);
        snrt_dma_start_1d(local_weight_scale_0, weight_scale, weight_scale_size);

#if NUM_CONTEXTS > 1
        snrt_dma_start_1d(local_input_matrix_1, input_matrix, input_mat_size);
        snrt_dma_start_1d(local_weight_matrix_1, weight_matrix, weight_mat_size);
        snrt_dma_start_1d(local_input_scale_1, input_scale, input_scale_size);
        snrt_dma_start_1d(local_weight_scale_1, weight_scale, weight_scale_size);
#endif
#if NUM_CONTEXTS > 2
        snrt_dma_start_1d(local_input_matrix_2, input_matrix, input_mat_size);
        snrt_dma_start_1d(local_weight_matrix_2, weight_matrix, weight_mat_size);
        snrt_dma_start_1d(local_input_scale_2, input_scale, input_scale_size);
        snrt_dma_start_1d(local_weight_scale_2, weight_scale, weight_scale_size);
#endif

        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();

    if (snrt_is_dm_core()) {
        printf("Running MXITA from core %d (%s)\r\n", core_idx, CORE_TYPE_STR());
        hwpe_soft_clear();

        printf("[cycle=%7u] Starting MXITA\r\n", snrt_mcycle());

        mxita_core_idx = core_idx;

        hwpe_set_perfcnt(NUM_CONTEXTS);

#if NUM_CONTEXTS > 1
        mxita_completed_runs = 0;
        snrt_interrupt_disable(IRQ_M_ACC);
#endif

        uint32_t start_cycle = snrt_mcycle();

        // Context 0
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

        hwpe_trigger_job();

#if NUM_CONTEXTS > 1
        // Context 1
        hwpe_wait_acquire_job();

        mxita_cfg(
            k_size, l_size, lk_size, 
            (uint32_t)local_input_matrix_1,
            (uint32_t)local_weight_matrix_1, 
            (uint32_t)local_output_matrix_1,
            (uint32_t)local_input_scale_1, 
            (uint32_t)local_weight_scale_1, 
            bf16_sel
        );

        hwpe_trigger_job();
#endif

#if NUM_CONTEXTS > 2
        // Context 2
        hwpe_wait_acquire_job();

        mxita_cfg(
            k_size, l_size, lk_size, 
            (uint32_t)local_input_matrix_2,
            (uint32_t)local_weight_matrix_2, 
            (uint32_t)local_output_matrix_2,
            (uint32_t)local_input_scale_2, 
            (uint32_t)local_weight_scale_2, 
            bf16_sel
        );

        hwpe_trigger_job();
#endif

#if NUM_CONTEXTS == 1
        snrt_wfi();
#else
        // we now handle all pending and future interrupts
        snrt_interrupt_enable(IRQ_M_ACC);

        while (mxita_completed_runs < NUM_CONTEXTS) {
            snrt_wfi();
        }
        mxita_completed_runs = 0;
#endif

        uint32_t end_cycle = snrt_mcycle();

        printf("[cycle=%7u] MXITA interrupt\r\n", snrt_mcycle());
        
        hw_cycles = hwpe_get_perfcnt();
        sw_cycles = end_cycle - start_cycle;
    }

    snrt_cluster_hw_barrier();

    if (core_idx == 0) {
        printf("MXITA Performance:\r\n");
        printf(" total: %u cycles\r\n", sw_cycles);
        printf(" HW:    %u cycles\r\n", hw_cycles);
        printf(" SW overhead: %u cycles (%.2f\%)\r\n", 
            sw_cycles - hw_cycles,
            100.f * (sw_cycles - hw_cycles) / sw_cycles
        );

        argsStruct->hw_cycles += hw_cycles;
        argsStruct->sw_cycles += sw_cycles;

        printf("--- Starting DUT vs REF comparison --- \r\n");

        int total_comparisons = output_mat_size / NBYTES_OUT_MAT;

        // for RTL, we just compare a few values
        if (argsStruct->is_rtl) {
            total_comparisons = 5;
        }

        printf("Performing %d comparisons...\r\n", total_comparisons);

        int errors = 0;
        float *out_float = (float *)local_output_matrix_0;
        uint16_t *out_bf16 = (uint16_t *)local_output_matrix_0;
        for (int i = 0; i < total_comparisons; i++) {
            float dut = bf16_sel ? uint32_to_float((uint32_t)out_bf16[i] << 16) : out_float[i];
            float ref = bf16_sel ? output_matrix[i ^ 1] : output_matrix[i];
            float err = dut - ref;
            float abs_err = fabs(err);
            float max_err = RELATIVE_TOLERANCE * fabs(ref);
            if (abs_err > max_err) {
                errors += 1;
                printf("DUT OUT VS REF OUT [%d]: %f vs %f\r\n", i, dut, ref);
            }
        }
        printf("Number of errors: %d over %d, %.2f%%\r\n", errors, total_comparisons,
               100.f * errors / total_comparisons);

        mxita_test_failed = (errors > 0);
    }

    snrt_cluster_hw_barrier();

    // not needed for FPGA (it was useful for verify.py)
    // if (snrt_is_dm_core()) {
    //     size_t bytes = sizeof(result);
    //     snrt_dma_start_1d((volatile void *)result,
    //                       (volatile void *)local_output_matrix,
    //                       bytes);
    //     snrt_dma_wait_all();
    // }
    // snrt_cluster_hw_barrier();

    return mxita_test_failed << 1;
}
