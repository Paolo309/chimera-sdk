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

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

#define MXITA_TCDM_ALIGN 64

#define M 8
#define N 4
#define P 4
#define Q 4

#define HWPE_ADDR_BASE 0x18040000
#define MXITA_TRIGGER 0x00
#define MXITA_ACQUIRE 0x04
#define HWPE_MXIP_ADDR (HWPE_ADDR_BASE + 0x58)
#define HWPE_WRITE(value, offset) *(int *)(HWPE_ADDR_BASE + offset) = value
#define HWPE_READ(offset) *(int *)(HWPE_ADDR_BASE + offset)

// tolerance for output comparison
#define RELATIVE_TOLERANCE 1e-2

// MXITA HWPE cfg
void mxita_cfg(uint8_t k_size, uint16_t l_size, uint8_t lk_size, unsigned int input_ptr,
               unsigned int weight_ptr, unsigned int output_ptr, unsigned int input_scale_ptr,
               unsigned int weight_scale_ptr, unsigned int bf16_sel) {
    uint32_t l_dims_reg = 0;
    uint32_t ctrl_stream_reg = 0;
    l_dims_reg = ((uint32_t)lk_size << 24) | ((uint32_t)l_size << 8) | ((uint32_t)k_size << 0);
    HWPE_WRITE(input_ptr, 0x20);
    HWPE_WRITE(weight_ptr, 0x24);
    HWPE_WRITE(output_ptr, 0x28);
    HWPE_WRITE(l_dims_reg, 0x2C);
    HWPE_WRITE(0, 0x30); // reg_ctrl_stream
    HWPE_WRITE(input_scale_ptr, 0x34);
    HWPE_WRITE(weight_scale_ptr, 0x38);
    HWPE_WRITE(bf16_sel, 0x3C);
}

static inline void hwpe_trigger_job() {
    HWPE_WRITE(0, MXITA_TRIGGER);
}

inline void snrt_hwpe_clr_mxip(uint32_t core_idx) {
    *(volatile uint32_t *)HWPE_MXIP_ADDR = (1 << core_idx);
}

static inline int hwpe_acquire_job() {
    return HWPE_READ(MXITA_ACQUIRE);
}

/**
 * @brief Reinterpret uint32_t as float (no conversion).
 *
 * @param b uint32_t value to convert.
 *
 * @returns float Converted float value.
 */
static inline float uint32_to_float(uint32_t b) {
    float f;
    memcpy(&f, &b, sizeof(f));
    return f;
}

SNRT_CLUSTER_L1_ZERO(void *local_input_matrix);
SNRT_CLUSTER_L1_ZERO(void *local_weight_matrix);
SNRT_CLUSTER_L1_ZERO(void *local_input_scale);
SNRT_CLUSTER_L1_ZERO(void *local_weight_scale);
SNRT_CLUSTER_L1_ZERO(void *local_output_matrix);

volatile int running_mxita = 0;
volatile int mxita_core_idx = 0;

/**
 * @brief L1 allocator allowing custom alignment.
 * Can be used interchangeably with default allocator.
 *
 * @param size Size of the allocation in bytes.
 * @param align Alignment of the allocation in bytes.
 *
 * @returns void* Pointer to the allocated memory.
 */
inline void *mxita_l1_alloc(size_t size, size_t align) {
    snrt_allocator_t *alloc = snrt_l1_allocator();

    size = ALIGN_UP(size, align);

    size_t pad = ALIGN_UP(alloc->next, align) - alloc->next;
    void *ret = (void *)(alloc->next + pad);
    alloc->next += size + pad;

    return ret;
}

/**
 * @brief Interrupt handler for the cluster, which clears the interrupt flag for the current hart.
 *
 * @warning Stack, thread and global pointer might not yet be set up!
 */
// __attribute__((naked))
void clusterInterruptHandler() {
    _SET_CLUSTER_BUSY();
    _SETUP_GP();

    // FIXME the interrupt should be naked (but this still works)
    if (running_mxita) {
        snrt_hwpe_clr_mxip(mxita_core_idx);
        running_mxita = 0;
    }

    asm volatile(
        // Load mhartid CSR into t0
        "csrr t0, mhartid\n"

        // Load clint base address into t1
        "la t1, __base_clint\n"

        // Calculate the interrupt target address: t1 = t1 + (t0 * 4)
        "slli t0, t0, 2\n"
        "add t1, t1, t0\n"
        // Store 0 to the interrupt target address
        "sw zero, 0(t1)\n"
        "ret"
        :            // No outputs
        :            // No inputs
        : "t0", "t1" // Declare clobbered registers
    );
}

/**
 * @brief Main function of the cluster test.
 *
 * @return int Return 0 if the test was successful, -1 otherwise.
 */
int32_t testReturn(void *args) {

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
        local_input_matrix = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN);
        local_weight_matrix = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN);
        local_input_scale = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN);
        local_weight_scale = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN);
        local_output_matrix = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN);

        snrt_dma_start_1d(local_input_matrix, input_matrix, input_mat_size);
        snrt_dma_start_1d(local_weight_matrix, weight_matrix, weight_mat_size);
        snrt_dma_start_1d(local_input_scale, input_scale, input_scale_size);
        snrt_dma_start_1d(local_weight_scale, weight_scale, weight_scale_size);

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
        mxita_cfg(k_size, l_size, lk_size, (unsigned int)local_input_matrix,
                  (unsigned int)local_weight_matrix, (unsigned int)local_output_matrix,
                  (unsigned int)local_input_scale, (unsigned int)local_weight_scale, bf16_sel);

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

        printf("Starting DUT vs REF comparison \r\n");

        int total_comparisons = output_mat_size / NBYTES_OUT_MAT;

        // for RTL, we just compare a few values
        if (argsStruct->is_rtl) {
            total_comparisons = 5;
        }

        printf("Performing %d comparisons...\r\n", total_comparisons);

        int errors = 0;
        float *out_float = (float *)local_output_matrix;
        uint16_t *out_bf16 = (uint16_t *)local_output_matrix;
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

    return 0;
}

int32_t testOtherCluster(void *args) {
    snrt_init();

    uint32_t core_idx = snrt_cluster_core_idx();

    snrt_int_clr_mcip();

    if (core_idx == 5) {
        printf("Hello from cluster %d with %d cores\r\n", snrt_cluster_idx(),
               _chimera_numCores[snrt_cluster_idx()]);
    }
    snrt_cluster_hw_barrier();

    return 0;
}
