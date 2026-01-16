// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

// Include Standard Libraries
#include <stdio.h>
#include <string.h>
#include <math.h>

// Include Application Headers
#include "test_cluster.h"
#include "test_host.h"
#include "data.h"
#include "mxita_util.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

static volatile int mxita_test_failed = 0;

SNRT_CLUSTER_L1_ZERO(static void *local_input1_matrix);
SNRT_CLUSTER_L1_ZERO(static void *local_weight1_matrix);
SNRT_CLUSTER_L1_ZERO(static void *local_input1_scale);
SNRT_CLUSTER_L1_ZERO(static void *local_weight1_scale);
SNRT_CLUSTER_L1_ZERO(static void *local_output1_matrix);

SNRT_CLUSTER_L1_ZERO(static void *local_input2_matrix);
SNRT_CLUSTER_L1_ZERO(static void *local_weight2_matrix);
SNRT_CLUSTER_L1_ZERO(static void *local_input2_scale);
SNRT_CLUSTER_L1_ZERO(static void *local_weight2_scale);
SNRT_CLUSTER_L1_ZERO(static void *local_output2_matrix);


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

    size = ALIGN_UP(size, align);

    size_t pad = ALIGN_UP(alloc->next, align) - alloc->next;
    void *ret = (void *)(alloc->next + pad);
    alloc->next += size + pad;

    return ret;
}

static volatile int running_mxita = 0;
static volatile int mxita_core_idx = 0;

/**
 * @brief Clear mxita's interrupt flag for the given core index.
 *
 * @param core_idx Core index to clear the interrupt flag for.
 */
static inline void snrt_hwpe_clr_mxip(uint32_t core_idx) {
    *(volatile uint32_t *)HWPE_MXIP_ADDR = (1 << core_idx);
}

// --------------------------------------------------------------------------

/**
 * @brief Interrupt handler for mxita, which clears the interrupt 
 * flag for the current core id.
 *
 * @warning Stack, thread and global pointer might not yet be set up!
 */
void clusterInterruptHandler_test_2cores() {
    _SET_CLUSTER_BUSY();
    _SETUP_GP();
    _CLEAR_MSIP();

    if (running_mxita) {
        snrt_hwpe_clr_mxip(mxita_core_idx);
        running_mxita = 0;
    }
}

/**
 * @brief Main function of the cluster test.
 *
 * @return int Return 0 if the test was successful, -1 otherwise.
 */
int32_t mxita_test_2cores(void *args) {

    /*
     * Initialize the Snitch runtime.
     */
    snrt_init();

    uint32_t core_idx = snrt_cluster_core_idx();

    // Clear interrupt from host
    snrt_int_clr_mcip();

    // Enable accelerator interrupts
    snrt_interrupt_enable(IRQ_M_ACC);

    if (core_idx == 0) {
        printf("Dual core tiles test\r\n");
        printf("Running MXITA on cluster %d with %d cores\r\n", snrt_cluster_idx(),
               _chimera_numCores[snrt_cluster_idx()]);
        printf("HWPE_ADDR_BASE = 0x%08X\r\n", HWPE_ADDR_BASE);
    }
    snrt_cluster_hw_barrier();

    offloadArgs_t *argsStruct = (offloadArgs_t *)args;

    uint32_t NBYTES_IW_MAT = sizeof(int8_t);
    uint32_t NBYTES_IW_SCALE = sizeof(uint8_t);
    uint32_t NBYTES_OUT_MAT = sizeof(float);

    // FP32 TO BF16
    uint32_t bf16_sel = argsStruct->bf16_sel;
    // uint32_t bf16_sel = 1;

    // DEFAULT
    uint8_t k_size = 8;
    uint16_t l_size = 64;
    uint8_t lk_size = 8;

    uint16_t input_mat_size = N * P * l_size * NBYTES_IW_MAT;
    uint16_t weight_mat_size = M * Q * l_size * NBYTES_IW_MAT;
    uint16_t input_scale_size =
        (N * P * lk_size * NBYTES_IW_SCALE < 512) ? 512 : N * P * lk_size * NBYTES_IW_SCALE;
    uint16_t weight_scale_size =
        (M * Q * lk_size * NBYTES_IW_SCALE < 512) ? 512 : M * Q * lk_size * NBYTES_IW_SCALE;
    uint16_t output_mat_size =
        bf16_sel ? M * N * P * Q * NBYTES_OUT_MAT / 2 : M * N * P * Q * NBYTES_OUT_MAT;

    if (core_idx == 0) {
        printf("(M, N, P, Q) = (%d, %d, %d, %d)\r\n", M, N, P, Q);
        printf("(K, L, LK)   = (%d, %d, %d)\r\n", k_size, l_size, lk_size);
        printf("bf16: %s\r\n", bf16_sel ? "ON" : "OFF");
    }

    if (snrt_is_dm_core()) {
        local_input1_matrix = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN);
        local_weight1_matrix = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN);
        local_input1_scale = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN);
        local_weight1_scale = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN);
        local_output1_matrix = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN);

        local_input2_matrix = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN);
        local_weight2_matrix = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN);
        local_input2_scale = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN);
        local_weight2_scale = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN);
        local_output2_matrix = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN);

        snrt_dma_start_1d(local_input1_matrix, input_matrix, input_mat_size);
        snrt_dma_start_1d(local_weight1_matrix, weight_matrix, weight_mat_size);
        snrt_dma_start_1d(local_input1_scale, input_scale, input_scale_size);
        snrt_dma_start_1d(local_weight1_scale, weight_scale, weight_scale_size);

        snrt_dma_start_1d(local_input2_matrix, input_matrix, input_mat_size);
        snrt_dma_start_1d(local_weight2_matrix, weight_matrix, weight_mat_size);
        snrt_dma_start_1d(local_input2_scale, input_scale, input_scale_size);
        snrt_dma_start_1d(local_weight2_scale, weight_scale, weight_scale_size);

        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();

    if (core_idx == 2) {
        printf("[cycle=%7u] Starting MXITA from core %d\r\n", snrt_mcycle(), core_idx);

        volatile int status1;
        do {
            status1 = hwpe_acquire_job();
        } while (status1 < 0);

        printf("[cycle=%7u] MXITA status %d acquired from core %d\r\n", snrt_mcycle(), status1,
               core_idx);

        // uint64_t t0 = (uint64_t)snrt_mcycle();

        // cast void pointer into int32 value
        mxita_cfg(k_size, l_size, lk_size, (unsigned int)local_input1_matrix,
                  (unsigned int)local_weight1_matrix, (unsigned int)local_output1_matrix,
                  (unsigned int)local_input1_scale, (unsigned int)local_weight1_scale, bf16_sel);

        printf("[cycle=%7u] MXITA configured from core %d\r\n", snrt_mcycle(), core_idx);

        running_mxita = 1; // to tell the interrupt handler to clear mxip
        mxita_core_idx = core_idx;

        volatile uint32_t start_cycle = snrt_mcycle();

        hwpe_trigger_job();
        snrt_wfi();

        // XXX not accurate, also accounts for interrupt handler
        volatile uint32_t end_cycle = snrt_mcycle();
        argsStruct->cycles = end_cycle - start_cycle;

        printf("[cycle=%7u] MXITA interrupt from core %d\r\n", snrt_mcycle(), core_idx);
        printf("cycles: %u\r\n", snrt_mcycle(), argsStruct->cycles);
    }

    if (core_idx == 3) {
        printf("[cycle=%7u] Starting MXITA from core %d\r\n", snrt_mcycle(), core_idx);

        volatile int status1;
        do {
            status1 = hwpe_acquire_job();
        } while (status1 < 0);

        printf("[cycle=%7u] MXITA status %d acquired from core %d\r\n", snrt_mcycle(), status1,
               core_idx);

        // uint64_t t0 = (uint64_t)snrt_mcycle();

        // cast void pointer into int32 value
        mxita_cfg(k_size, l_size, lk_size, (unsigned int)local_input2_matrix,
                  (unsigned int)local_weight2_matrix, (unsigned int)local_output2_matrix,
                  (unsigned int)local_input2_scale, (unsigned int)local_weight2_scale, bf16_sel);

        printf("[cycle=%7u] MXITA configured from core %d\r\n", snrt_mcycle(), core_idx);

        running_mxita = 1; // to tell the interrupt handler to clear mxip
        mxita_core_idx = core_idx;

        volatile uint32_t start_cycle = snrt_mcycle();

        hwpe_trigger_job();
        snrt_wfi();

        // XXX not accurate, also accounts for interrupt handler
        volatile uint32_t end_cycle = snrt_mcycle();
        argsStruct->cycles = end_cycle - start_cycle;

        printf("[cycle=%7u] MXITA interrupt from core %d\r\n", snrt_mcycle(), core_idx);
        printf("cycles: %u\r\n", snrt_mcycle(), argsStruct->cycles);
    }

    snrt_cluster_hw_barrier();

    if (core_idx == 4) {
        printf("Starting DUT vs REF comparison \r\n");

        int total_comparisons = output_mat_size / NBYTES_OUT_MAT;

        // for RTL, we just compare a few values
        if (argsStruct->is_rtl) {
            total_comparisons = 5;
        }

        printf("Performing %d comparisons...\r\n", total_comparisons);

        int errors = 0;
        float *out_float = (float *)local_output1_matrix;
        uint16_t *out_bf16 = (uint16_t *)local_output1_matrix;
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
