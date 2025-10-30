// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Moritz Scherer <scheremo@iis.ee.ethz.ch>

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

// #if MIN_CHUNK_SIZE != 64
// #error "MIN_CHUNK_SIZE must be 64"
// #endif
#define MXITA_TCDM_ALIGN 64

#define M 8
#define N 4
#define P 4
#define Q 4

#define HWPE_ADDR_BASE 0x40040000 // corresponds to `ext_mem_start_address` in snitch_cluster
#define MXITA_TRIGGER 0x00
#define MXITA_ACQUIRE 0x04
#define HWPE_MXIP_ADDR (HWPE_ADDR_BASE + 0x58)
#define HWPE_WRITE(value, offset) *(int *)(HWPE_ADDR_BASE + offset) = value
#define HWPE_READ(offset) *(int *)(HWPE_ADDR_BASE + offset)

// MXITA HWPE cfg
void mxita_cfg(uint8_t k_size, uint16_t l_size, uint8_t lk_size, unsigned int input_ptr, unsigned int weight_ptr, unsigned int output_ptr, unsigned int input_scale_ptr, unsigned int weight_scale_ptr) {
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
}

static inline void hwpe_trigger_job() { HWPE_WRITE(0, MXITA_TRIGGER); }

inline void snrt_hwpe_clr_mxip(uint32_t core_idx) {
    * (volatile uint32_t*)HWPE_MXIP_ADDR = (1 << core_idx);
}

static inline int hwpe_acquire_job() { return HWPE_READ(MXITA_ACQUIRE); }

int mxita_compare_float(float* dut_output, float* ref_output, int array_len){
  int errors = 0;
  for (int i = 0; i < array_len; i++){
    if ((dut_output[i] / ref_output[i] < 0.99)  || (dut_output[i] / ref_output[i] > 1.01 )) {
      errors += 1;
    }
  }
  return errors;
}

void* __attribute__((__section__(".cbss"))) local_input_matrix;
void* __attribute__((__section__(".cbss"))) local_weight_matrix;
void* __attribute__((__section__(".cbss"))) local_input_scale;
void* __attribute__((__section__(".cbss"))) local_weight_scale;
void* __attribute__((__section__(".cbss"))) local_output_matrix;

volatile int running_mxita = 0;

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
    _SETUP_GP();

    if (running_mxita) {
        snrt_hwpe_clr_mxip(2); // TODO just get it from somewhere (e.g. snrt_cluster_core_idx, or variable)
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

int32_t __attribute__((__section__(".cbss"))) * pointer_input;
int32_t __attribute__((__section__(".cbss"))) * pointer_output;



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

    static volatile uint64_t prog_cycles = 0;
    static volatile uint64_t mxita_cycles  = 0;

    // Clear interrupt from host
    snrt_int_clr_mcip();

    // Enable accelerator interrupts
    snrt_interrupt_enable(IRQ_M_ACC);

    uint32_t NBYTES_IW_MAT = sizeof(int8_t);
    uint32_t NBYTES_IW_SCALE = sizeof(uint8_t);
    uint32_t NBYTES_OUT_MAT = sizeof(float);

    // DEFAULT
    uint8_t k_size = 8;
    uint16_t l_size = 64;
    uint8_t lk_size = 8;

    uint16_t input_mat_size = N*P*l_size*NBYTES_IW_MAT;
    uint16_t weight_mat_size = M*Q*l_size*NBYTES_IW_MAT;
    uint16_t input_scale_size = (N*P*lk_size*NBYTES_IW_SCALE < 512) ? 512 : N*P*lk_size*NBYTES_IW_SCALE;
    uint16_t weight_scale_size = (M*Q*lk_size*NBYTES_IW_SCALE < 512) ? 512 : M*Q*lk_size*NBYTES_IW_SCALE;
    uint16_t output_mat_size = M*N*P*Q*NBYTES_OUT_MAT;


    offloadArgs_t *argsStruct = (offloadArgs_t *)args;

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
        printf("[cycle=%u] Starting MXITA from core %d\n", snrt_mcycle(), core_idx);
        
        volatile int status1;
        do {
            status1 = hwpe_acquire_job();
        } while (status1 < 0);

        printf("[cycle=%u] MXITA status %d acquired from core %d\n", snrt_mcycle(), status1, core_idx);

        //uint64_t t0 = (uint64_t)snrt_mcycle();

        // cast void pointer into int32 value
        mxita_cfg(
            k_size, l_size, lk_size,
            (unsigned int) local_input_matrix,
            (unsigned int) local_weight_matrix,
            (unsigned int) local_output_matrix,
            (unsigned int) local_input_scale,
            (unsigned int) local_weight_scale
        );

        // uint64_t t1 = (uint64_t)snrt_mcycle();
        // prog_cycles = t1 - t0;

        printf("[cycle=%u] MXITA configured from core %d\n", snrt_mcycle(), core_idx);

        // uint32_t start_cycle = snrt_mcycle();

        running_mxita = 1; // to tell the interrupt handler to clear mxip
        hwpe_trigger_job();

        // insert some nops to delay the core
        // for (volatile int i = 0; i < 1800; i++) {
        //     asm volatile("nop");
        // }

        // uint64_t t2 = (uint64_t)snrt_mcycle();

        snrt_wfi();

        // uint64_t t3 = (uint64_t)snrt_mcycle();
        // mxita_cycles = t3 - t2;

        // snrt_hwpe_clr_mxip(core_idx); // DONE IN THE INTERRUPT HANDLER

        printf("[cycle=%u] MXITA interrupt from core %d\n", snrt_mcycle(), core_idx);

        // printf("[cycle=%u] MXITA interrupt clear from core %d\n", snrt_mcycle(), core_idx);

        // uint32_t finish_cycle = snrt_mcycle();

        // printf("[cycle=%u] Checking MXITA from core %d\n", snrt_mcycle(), core_idx);

        // printf("Starting DUT vs REF comparison \n");
        // int errors = 0;
        // float *out = (float*) local_output_matrix;
        // for (int i = 0; i < M*N*P*Q; i++) {
        //     if (i % 32==0) printf("Current i is %d\n", i);
        //     float dut = out[i];
        //     float ref = output_matrix[i];
        //     if ((dut / ref < 0.99)  || (dut / ref > 1.01 )) {
        //         errors += 1;
        //         printf("DUT OUT VS REF OUT: %f vs %f\n", dut, ref);
        //     }
        // }
        // printf("Number of errors: %d\n", errors);
    }

    snrt_cluster_hw_barrier();
    
    if (snrt_is_dm_core()) {        
        size_t bytes = sizeof(result);
        snrt_dma_start_1d((volatile void *)result,
                          (volatile void *)local_output_matrix,
                          bytes);
        snrt_dma_wait_all();
    }

    snrt_cluster_hw_barrier();

    return 0;
}
