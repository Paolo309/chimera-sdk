// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Moritz Scherer <scheremo@iis.ee.ethz.ch>

// Include Standard Libraries
#include <stdio.h>
#include <string.h>

// Include Application Headers
#include "test_cluster.h"
#include "test_host.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// Include Runtime Headers
#include "snrt.h"

// Include MXITA Headers
#include "data.h"

/**
 * @brief Interrupt handler for the cluster, which clears the interrupt flag for the current hart.
 *
 * @warning Stack, thread and global pointer might not yet be set up!
 */
__attribute__((naked)) void clusterInterruptHandler() {
    _SETUP_GP_TP();

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

#define M 8
#define N 4
#define P 4
#define Q 4

// #define HWPE_ADDR_BASE 0x30000000
#define HWPE_ADDR_BASE 0x30040000
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

volatile int status1;
void *local_input_matrix;
void *local_weight_matrix;
void *local_input_scale;
void *local_weight_scale;
void *local_output_matrix;

#define IRQ_M_ACC     20

/**
 * @brief Main function of the cluster test.
 *
 * @return int Return 0 if the test was successful, -1 otherwise.
 */
int32_t testReturn(void *args) {
    // Cast to the correct struct
    offloadArgs_t *argsStruct = (offloadArgs_t *)args;

    // Check if the value is correct
    if (argsStruct->value != 0xdeadbeef) {
        return -1;
    }

    uint32_t core_idx = snrt_cluster_core_idx();

    static volatile uint64_t prog_cycles = 0;
    static volatile uint64_t mxita_cycles  = 0;

    // Clear interrupt from host
    snrt_int_clr_mcip();

    // Enable accelerator interrupts
    // snrt_interrupt_enable(IRQ_M_ACC);
    // printf("IRQ_M_ACC: %d\n", IRQ_M_ACC);

    uint32_t NBYTES_IW_MAT = sizeof(int8_t);
    uint32_t NBYTES_IW_SCALE = sizeof(uint8_t);
    uint32_t NBYTES_OUT_MAT = sizeof(float);

    // DEFAULT
    uint8_t k_size = 8;
    uint16_t l_size = 64;
    uint8_t lk_size = 8;

    // uint16_t input_mat_size = N*P*l_size*NBYTES_IW_MAT;
    // uint16_t weight_mat_size = M*Q*l_size*NBYTES_IW_MAT;
    // uint16_t input_scale_size = (N*P*lk_size*NBYTES_IW_SCALE < 512) ? 512 : N*P*lk_size*NBYTES_IW_SCALE;
    // uint16_t weight_scale_size = (M*Q*lk_size*NBYTES_IW_SCALE < 512) ? 512 : M*Q*lk_size*NBYTES_IW_SCALE;
    // uint16_t output_mat_size = M*N*P*Q*NBYTES_OUT_MAT;

    argsStruct->mxita_return[core_idx].core_id = core_idx;
    if (snrt_is_dm_core()) {
        // local_input_matrix = snrt_l1_alloc(input_mat_size);
        // local_weight_matrix = snrt_l1_alloc(weight_mat_size);
        // local_input_scale = snrt_l1_alloc(input_scale_size);
        // local_weight_scale = snrt_l1_alloc(weight_scale_size);
        // local_output_matrix = snrt_l1_alloc(output_mat_size);

        // snrt_dma_start_1d(local_input_matrix, input_matrix, input_mat_size);
        // snrt_dma_start_1d(local_weight_matrix, weight_matrix, weight_mat_size);
        // snrt_dma_start_1d(local_input_scale, input_scale, input_scale_size);
        // snrt_dma_start_1d(local_weight_scale, weight_scale, weight_scale_size);

        // snrt_dma_wait_all();
        
        argsStruct->mxita_return[core_idx].is_dm_core = 1;
    } else {
        argsStruct->mxita_return[core_idx].is_dm_core = 0;
    }

    snrt_cluster_hw_barrier();

    if (core_idx == 2) {
        argsStruct->mxita_return[core_idx].core_id = core_idx * 100;
    }

    return 0;
}

// snrt_l1_start_addr();

    // snrt_cls_base_addr();

    // extern volatile uint32_t __tdata_start, __tdata_end;
    // extern volatile uint32_t __tbss_start, __tbss_end;

    // size_t size;
    // volatile uint32_t tls_ptr;

    // // To avoid contentions in main memory, and take advantage of the
    // // bandwidth of the DMA, the DM core initializes the TLS section
    // // for every core in a cluster.
    // if (snrt_is_dm_core()) {
    //     size = (size_t)(&__tdata_end) - (size_t)(&__tdata_start);

    //     // First initialize the DM core's .tdata section from main memory
    //     asm volatile("mv %0, tp" : "=r"(tls_ptr) : :);
    //     snrt_dma_start_1d((void *)tls_ptr, (void *)(&__tdata_start), size);

    //     snrt_dma_wait_all();
    // }

    // snrt_cluster_hw_barrier();

    // // *(volatile uint32_t *)(long)(0x03004000) = 'a';