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

#define MXITA_L 128
#define MXITA_K 32
#include "data.h"

#define NUM_CONTEXTS 1
// #define NO_HANDLER

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

#define TRACE
#define TRACE_ALLOC

#define NUM_MANTA_RUNS 1

static const float RELATIVE_TOLERANCE = 1e-2;

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

// Performance measures
SNRT_CLUSTER_L1_ZERO(static uint32_t hw_cycles);
SNRT_CLUSTER_L1_ZERO(static uint32_t hw_cycles_2nd_run);
SNRT_CLUSTER_L1_ZERO(static uint32_t sw_cycles);
SNRT_CLUSTER_L1_ZERO(static uint32_t bs_cycles);
SNRT_CLUSTER_L1_ZERO(static uint32_t bs_cycles_2nd_run);
SNRT_CLUSTER_L1_ZERO(static uint32_t sw_start);
SNRT_CLUSTER_L1_ZERO(static uint32_t sw_end);
SNRT_CLUSTER_L1_ZERO(static uint32_t finish_launching);


SNRT_CLUSTER_L1_ZERO(static void *run_counter_pointer);
SNRT_CLUSTER_L1_ZERO(static int touched);

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
SNRT_CLUSTER_L1_ZERO(static volatile int mxita_completed_runs);
#endif

/**
 * @brief Custom interrupt handler for mxita, which clears the interrupt.
 */
static void hwpeInterruptHandler() { 
    // _CLEAR_MSIP();

#ifdef NO_HANDLER
    touched = 1; // to check whether the handler is actually called
#endif

    // snrt_hwpe_clr_mxip(snrt_cluster_core_idx());
    // snrt_hwpe_clr_mxip(0);
    snrt_hwpe_clr_mxip(mxita_core_idx);
#if NUM_CONTEXTS > 1
    // ++mxita_completed_runs;
    (*(volatile int*)run_counter_pointer)++;
#endif
    // csrrc zero, mie, 0xffffffff
    // asm volatile(
    //     "li   t1, -1\n"        // 0xFFFF_FFFF
    //     "csrrc zero, mie, t1\n"// clear all bits in mie
    //     "mret\n"
    //     ::: "t1", "memory"
    // );
    _CLEAR_MSIP();
    // asm volatile(
    //     "mret\n"
    //     ::: "t1", "memory"
    // );
}
// __attribute__((naked, aligned(4)))
// void hwpeInterruptHandler(void) {
//   asm volatile(
//     // hartid -> t0
//     "csrr  t0, mhartid\n"

//     // Clear MSIP: *(CLINT_MSIP_BASE + 4*hartid) = 0
//     "li    t1, %[msip_base]\n"
//     "slli  t2, t0, 2\n"
//     "add   t1, t1, t2\n"
//     "sw    zero, 0(t1)\n"

//     // Compute local core index: core = hartid - %[hart_base]
//     "addi  t0, t0, -%[hart_base]\n"

//     // mask = 1 << core
//     "li    t1, 1\n"
//     "sll   t1, t1, t0\n"

//     // Ack HWPE: *(HWPE_MXIP_ADDR) = mask
//     "li    t2, %[hwpe_mxip]\n"
//     "sw    t1, 0(t2)\n"

//     // Return from C handler (NOT mret, because your trap vector does mret)
//     "ret\n"
//     :
//     : [msip_base] "i"(/* CLINT_MSIP_BASE */),
//       [hwpe_mxip] "i"(/* HWPE_MXIP_ADDR */),
//       [hart_base] "i"(/* base hartid for cluster cores */)
//     : "t0", "t1", "t2", "memory"
//   );
// }


__attribute__((naked, aligned(16)))
void _my_trap_vector() {
    asm volatile(
        // "li t0, 0x30001000\n"
        // "lw t0, 0x08(t0)\n"
        "la t0, hwpeInterruptHandler\n"
        "jalr t0\n"
        "mret\n"
        ::: "t0"
    );
    // asm volatile(
    //     "call hwpeInterruptHandler\n"
    //     "mret\n"
    //     ::: "t0"
    // );
}



__attribute__((naked, aligned(16)))
void fast_trap_vector() {
    asm volatile(
        "addi sp, sp, -144\n"
        "sw ra,   0(sp)\n"
        "sw t0,   4(sp)\n"
        "sw t1,   8(sp)\n"
        "csrr t0, mhartid\n" /* Load mhartid CSR into t0 */
        "la t1, __base_clint\n" /* Load clint base address into t1 */
        "slli t0, t0, 2\n" /* Calculate the interrupt target address: t1 = t1 + (t0 * 4) */
        "add t1, t1, t0\n"
        "sw zero, 0(t1)\n" /* Store 0 to the interrupt target address */
        "lw ra,   0(sp)\n"
        "lw t0,   4(sp)\n"
        "lw t1,   8(sp)\n"
        "addi sp, sp, 144\n"
        "mret\n"
        :            /* No outputs */
        :            /* No inputs */
        : "t0", "t1" /* Declare clobbered registers */
    );
}


// __attribute__((naked, aligned(16)))
// void fast_trap_vector() {
//     asm volatile(
//         // "li t0, 0x30001000\n"
//         // "lw t0, 0x08(t0)\n"
//         "la t0, hwpeInterruptHandler\n"
//         "jalr t0\n"
//         "mret\n"
//         ::: "t0"
//     );
//     // asm volatile(
//     //     "csrr t0, mhartid\n" /* Load mhartid CSR into t0 */
//     //     "la t1, __base_clint\n" /* Load clint base address into t1 */
//     //     "slli t0, t0, 2\n" /* Calculate the interrupt target address: t1 = t1 + (t0 * 4) */
//     //     "add t1, t1, t0\n"
//     //     "sw zero, 0(t1)\n" /* Store 0 to the interrupt target address */
//     //     "mret\n"
//     //     :            /* No outputs */
//     //     :            /* No inputs */
//     //     : "t0", "t1" /* Declare clobbered registers */
//     // );
//     // asm volatile(
//     //     "mret\n"
//     //     ::: "t0"
//     // );
// }


__attribute__((naked, aligned(4)))
void my_trap_entry(void) {
  asm volatile(
    // Allocate trap frame: 32 regs + 3 CSRs = 35 words = 140 bytes.
    // Round up to 144 to keep 16B alignment.
    "addi sp, sp, -144\n"

    // Save GPRs (x1..x31). x0 is hardwired 0.
    "sw ra,   0(sp)\n"
    // "sw gp,   4(sp)\n"
    // "sw tp,   8(sp)\n"
    "sw t0,  12(sp)\n"
    // "sw t1,  16(sp)\n"
    // "sw t2,  20(sp)\n"
    // "sw s0,  24(sp)\n"
    // "sw s1,  28(sp)\n"
    // "sw a0,  32(sp)\n"
    // "sw a1,  36(sp)\n"
    // "sw a2,  40(sp)\n"
    // "sw a3,  44(sp)\n"
    // "sw a4,  48(sp)\n"
    // "sw a5,  52(sp)\n"
    // "sw a6,  56(sp)\n"
    // "sw a7,  60(sp)\n"
    // "sw s2,  64(sp)\n"
    // "sw s3,  68(sp)\n"
    // "sw s4,  72(sp)\n"
    // "sw s5,  76(sp)\n"
    // "sw s6,  80(sp)\n"
    // "sw s7,  84(sp)\n"
    // "sw s8,  88(sp)\n"
    // "sw s9,  92(sp)\n"
    // "sw s10, 96(sp)\n"
    // "sw s11,100(sp)\n"
    // "sw t3, 104(sp)\n"
    // "sw t4, 108(sp)\n"
    // "sw t5, 112(sp)\n"
    // "sw t6, 116(sp)\n"

    // Save CSRs you might want for debug/dispatch (optional but useful)
    // "csrr t0, mepc\n"
    // "sw   t0, 120(sp)\n"
    // "csrr t0, mcause\n"
    // "sw   t0, 124(sp)\n"
    // "csrr t0, mtval\n"
    // "sw   t0, 128(sp)\n"

    // Call C handler (must be leaf-ish: no mret)
    // "call hwpeInterruptHandler_c\n"
    // "call hwpeInterruptHandler\n"
    
    "la t0, hwpeInterruptHandler\n"
    "jalr t0\n"

    // "li t0, 0x30001000\n"
    // "lw t0, 0x08(t0)\n"
    // "jalr t0\n"

    // Restore CSRs if you modified them (usually you don't touch mepc)
    // (skip unless you change mepc/mstatus in handler)

    // Restore GPRs
    "lw ra,   0(sp)\n"
    // "lw gp,   4(sp)\n"
    // "lw tp,   8(sp)\n"
    "lw t0,  12(sp)\n"
    // "lw t1,  16(sp)\n"
    // "lw t2,  20(sp)\n"
    // "lw s0,  24(sp)\n"
    // "lw s1,  28(sp)\n"
    // "lw a0,  32(sp)\n"
    // "lw a1,  36(sp)\n"
    // "lw a2,  40(sp)\n"
    // "lw a3,  44(sp)\n"
    // "lw a4,  48(sp)\n"
    // "lw a5,  52(sp)\n"
    // "lw a6,  56(sp)\n"
    // "lw a7,  60(sp)\n"
    // "lw s2,  64(sp)\n"
    // "lw s3,  68(sp)\n"
    // "lw s4,  72(sp)\n"
    // "lw s5,  76(sp)\n"
    // "lw s6,  80(sp)\n"
    // "lw s7,  84(sp)\n"
    // "lw s8,  88(sp)\n"
    // "lw s9,  92(sp)\n"
    // "lw s10, 96(sp)\n"
    // "lw s11,100(sp)\n"
    // "lw t3, 104(sp)\n"
    // "lw t4, 108(sp)\n"
    // "lw t5, 112(sp)\n"
    // "lw t6, 116(sp)\n"

    "addi sp, sp, 144\n"
    "mret\n"
    ::: "memory"
  );
}



static inline void dirty_wfi(void) {
//   asm volatile(
//     "wfi"
//     :
//     :
//     : "memory",
//       "ra",
//       "t0","t1","t2","t3","t4","t5","t6",
//       "a0","a1","a2","a3","a4","a5","a6","a7",
//       "ft0","ft1","ft2","ft3","ft4","ft5","ft6","ft7","ft8","ft9","ft10","ft11",
//       "fa0","fa1","fa2","fa3","fa4","fa5","fa6","fa7"
//   );
    asm volatile(
        "wfi"
        :
        :
        : "memory",
        "ra",
        "t0","t1","t2","t3","t4","t5","t6",
        "a0","a1","a2","a3","a4","a5","a6","a7"
    );
}

__attribute__((aligned(32)))
int32_t run_mxita(uint8_t lk_size, uint32_t bf16_sel) {
#if NUM_CONTEXTS > 1
    mxita_completed_runs = 0;
    *(volatile int*)run_counter_pointer = 0;
    snrt_interrupt_disable(IRQ_M_ACC);
    printf("run_counter_pointer = %p (value = %d)\r\n", &mxita_completed_runs, mxita_completed_runs);
#endif

    volatile int32_t mxita_core = snrt_cluster_core_idx();
    volatile int32_t start = snrt_mcycle();

    // Context 0 -------------------------------------------
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
    // Context 1 -------------------------------------------
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
    // Context 2 -------------------------------------------
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

    // #####################################################

#ifdef NO_HANDLER
    snrt_wfi();
    snrt_hwpe_clr_mxip(mxita_core);
#if NUM_CONTEXTS > 1
    for (volatile int i = 0; i < 7; i++) {
        asm volatile("nop");
    }
    snrt_wfi();
    snrt_hwpe_clr_mxip(mxita_core);
#endif

#if NUM_CONTEXTS > 2
    for (volatile int i = 0; i < 7; i++) {
        asm volatile("nop");
    }
    snrt_wfi();
    snrt_hwpe_clr_mxip(mxita_core);
#endif

#else // NO_HANDLER not defined

#if NUM_CONTEXTS == 1
    dirty_wfi();
#else
    snrt_interrupt_enable(IRQ_M_ACC);
    // Wait until all contexts have completed their runs
    // while (mxita_completed_runs < 3) {
    while (*(volatile int*)run_counter_pointer < NUM_CONTEXTS) {
        asm volatile("nop" ::: "memory");
        dirty_wfi();
        asm volatile("nop" ::: "memory");
    }
#endif
#endif

    asm volatile("nop" ::: "memory");
    asm volatile("" ::: "memory");
    volatile int32_t end = snrt_mcycle();

    printf("start = %u, end = %u\r\n", start, end);

    return end - start;
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

    uint32_t run_concurrent_tcdm = argsStruct->run_concurrent_tcdm;

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


    // ##########################
    // #      TCDM and DMA      #
    // ##########################

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

        local_input_matrix_0 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_weight_matrix_0 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_input_scale_0 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_weight_scale_0 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_output_matrix_0 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;

#if NUM_CONTEXTS > 1
        local_input_matrix_1 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_weight_matrix_1 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_input_scale_1 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_weight_scale_1 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_output_matrix_1 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
#endif

#if NUM_CONTEXTS > 2
        local_input_matrix_2 = mxita_l1_alloc(input_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_weight_matrix_2 = mxita_l1_alloc(weight_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_input_scale_2 = mxita_l1_alloc(input_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_weight_scale_2 = mxita_l1_alloc(weight_scale_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
        local_output_matrix_2 = mxita_l1_alloc(output_mat_size, MXITA_TCDM_ALIGN) - 0x40000000u + 0x18000000u;
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

        uintptr_t tcdm_base = snrt_align_up_hyperbank((uintptr_t)(0x18000000u + (snrt_l1_next() - 0x40000000u)));
        uintptr_t hyperbank0_base = tcdm_base;
        uintptr_t hyperbank1_base = tcdm_base + (SNRT_TCDM_SIZE / 2);

        // printf("TCDM base: %#x\r\n", tcdm_base);
        // printf("  Hyperbank 0 base: %#x\r\n", hyperbank0_base);
        // printf("  Hyperbank 1 base: %#x\r\n", hyperbank1_base);

        run_counter_pointer = (void *)hyperbank1_base;
        
        // XXX check cores' stack sizes to avoid overwriting any stack
        // local_test_buffer = (void *)hyperbank1_base; // OTHER HB
        // local_test_buffer = mxita_l1_alloc(1024, 512) - 0x40000000u + 0x18000000u; // SAME HB
    }

     

    snrt_cluster_hw_barrier();


    if (core_idx == 0) {
        // // asm volatile(
        // //     "la t0, _my_trap_vector\n"
        // //     "csrw mtvec, t0\n"
        // //     ::: "t0", "memory"
        // // );

        // // XXX This is the good one
        // asm volatile(
        //     "la t0, my_trap_entry\n"
        //     "csrw mtvec, t0\n"
        //     ::: "t0", "memory"
        // );

        // // asm volatile(
        // //     "la t0, hwpeInterruptHandler\n"
        // //     "csrw mtvec, t0\n"
        // //     ::: "t0", "memory"
        // // );

        // // asm volatile(
        // //     "la t0, fast_trap_vector\n"
        // //     "csrw mtvec, t0\n"
        // //     ::: "t0", "memory"
        // // );

        


        printf("Running MXITA from core %d (%s)\r\n", core_idx, CORE_TYPE_STR());
        printf("NUM JOBS = %d\r\n", NUM_CONTEXTS);
        hwpe_soft_clear();

        printf("[cycle=%7u] Starting MXITA\r\n", snrt_mcycle());

        // hwpeInterruptHandler(); // make sure no stale pending interrupt is latched

        // soft clear
        // hwpe_soft_clear();

#ifdef NO_HANDLER
        snrt_interrupt_global_disable(); // XXX uncomment to disable interrupt handling
#endif

        snrt_interrupt_enable(IRQ_M_ACC);
         
        // set_mie(0);

        // warm up run
        // run_mxita(lk_size, bf16_sel);
        // sw_start = 0;
        // sw_end = 0;
        // hwpe_soft_clear();

        mxita_core_idx = core_idx;

        hwpe_set_perfcnt(NUM_CONTEXTS);
        hwpe_set_perfcnt_busy(NUM_CONTEXTS);
        hwpe_set_perfcnt_2nd_run(NUM_CONTEXTS);
        hwpe_set_perfcnt_busy_2nd_run(NUM_CONTEXTS);

        sw_cycles = run_mxita(lk_size, bf16_sel);
        hw_cycles = hwpe_get_perfcnt();
        hw_cycles_2nd_run = hwpe_get_perfcnt_2nd_run();
        bs_cycles = hwpe_get_perfcnt_busy();
        bs_cycles_2nd_run = hwpe_get_perfcnt_busy_2nd_run();

        snrt_interrupt_disable(IRQ_M_ACC);
        snrt_interrupt_global_enable();
    
// #if NUM_CONTEXTS > 1
//         // mxita_completed_runs = 0;
//         *(volatile int*)run_counter_pointer = 0;
// #endif

        // printf("Trigger latency:               %u cycles\r\n", trigger_start - sw_start);
        // printf("Execution + interrupt latency: %u cycles\r\n", sw_end - trigger_start);

        // hammer_stop = 1;

        printf("[cycle=%7u] MXITA interrupt\r\n", snrt_mcycle());
        printf("[cycle=%7u] touched = %d\r\n", snrt_mcycle(), touched);
        // printf("sw start: %u, sw end: %u\r\n", sw_start, sw_end);
        // printf("Configuration latency: %u cycles\r\n", finish_launching - sw_start);
        // printf("Interrupt latency: %u cycles\r\n", sw_end - finish_launching);
    }

    snrt_cluster_hw_barrier();


    // ###############################
    // #      OUTPUT COMPARISON      #
    // ###############################

    if (core_idx == 0) {
        float avg_hw_cycles = (float)hw_cycles / (float)NUM_MANTA_RUNS;
        float avg_hw_cycles_2nd_run = (float)hw_cycles_2nd_run / (float)NUM_MANTA_RUNS;
        float avg_sw_cycles = (float)sw_cycles / (float)NUM_MANTA_RUNS;
        float avg_bs_cycles = (float)bs_cycles / (float)NUM_MANTA_RUNS;
        float avg_bs_cycles_2nd_run = (float)bs_cycles_2nd_run / (float)NUM_MANTA_RUNS;

        // float considered_frequency = argsStruct->frequency;
        float considered_frequency = 1e9;

        uint32_t FLOPs = NUM_CONTEXTS * 2 * M * N * P * Q * l_size;
        uint32_t MACs = FLOPs / 2;

        // ---
        // float flops_cycle = (float)FLOPs / avg_sw_cycles;
        // float macs_cycle = (float)MACs / avg_sw_cycles;
        // ---
        float flops_cycle = (float)FLOPs / avg_hw_cycles;
        float macs_cycle = (float)MACs / avg_hw_cycles;
        // ---
        // float flops_cycle = (float)FLOPs / avg_bs_cycles;
        // float macs_cycle = (float)MACs / avg_bs_cycles;

        float flops_second = flops_cycle * considered_frequency;
        float macs_second = macs_cycle * considered_frequency;

        printf("MXITA Performance:\r\n");
        printf("  total: %.2f cycles\r\n", avg_sw_cycles);
        printf("  HW:    %.2f cycles\r\n", avg_hw_cycles);
        printf("  HW 2:  %.2f cycles\r\n", avg_hw_cycles_2nd_run);
        printf("  BS:    %.2f cycles\r\n", avg_bs_cycles);
        printf("  BS 2:  %.2f cycles\r\n", avg_bs_cycles_2nd_run);
        printf("  SW oh: %.2f cycles (%.2f\%)\r\n", 
            avg_sw_cycles - avg_hw_cycles,
            100.f * (avg_sw_cycles - avg_hw_cycles) / avg_sw_cycles
        );
        printf("  HW perc: %.2f\%\r\n", 
            100.f * avg_hw_cycles / avg_sw_cycles
        );
        printf("  Total FLOPs: %u\r\n", FLOPs);
        printf("  Total MACs:  %u\r\n", MACs);
        printf("  Performance: %.2f FLOPs/cycle, %.2f MACs/cycle\r\n", flops_cycle, macs_cycle);
        printf("@%.2f MHz:\r\n", considered_frequency / 1e6);
        printf("  Performance: %.2f GFLOPs/s, %.2f GMACs/s\r\n", flops_second / 1e9, macs_second / 1e9);

        argsStruct->hw_cycles += avg_hw_cycles;
        argsStruct->sw_cycles += avg_sw_cycles;

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
            float abs_err = fabsf(err);
            float max_err = RELATIVE_TOLERANCE * fabsf(ref);
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
