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

#define TRACE

#define PERF_CNT_TCDM_ACCESSED 1
#define PERF_CNT_DMA_R_BW 0x15
#define PERF_CNT_DMA_W_BW 0x17
#define PERF_CNT_DMA_BUSY 0x19

#define L2_BUFFER_SIZE (16+8)*1024
uint8_t l2_buffer[L2_BUFFER_SIZE] = {0};


typedef struct {
    uint32_t transfer_cycles;
    float transfer_bandwidth_cycles;
    float transfer_bandwidth_seconds;
} transfer_perf_t;

typedef struct {
    uint32_t transfer_bytes;
    transfer_perf_t perf_sw;
    transfer_perf_t perf_cnt;
} transfer_test_result_t;

void print_test_result(transfer_test_result_t *result) {
    printf(" Buffer size:     %u bytes (%.2f KiB)\r\n", result->transfer_bytes, result->transfer_bytes / 1024.f);
    printf(" [SW] Cycles:     %u cycles\r\n", result->perf_sw.transfer_cycles);
    printf(" [SW] Bandwidth:  %.2f MiB/s (%.2f B/cycle)\r\n", result->perf_sw.transfer_bandwidth_seconds, result->perf_sw.transfer_bandwidth_cycles);
    printf(" [CNT] Cycles:    %u cycles\r\n", result->perf_cnt.transfer_cycles);
    printf(" [CNT] Bandwidth: %.2f MiB/s (%.2f B/cycle)\r\n", result->perf_cnt.transfer_bandwidth_seconds, result->perf_cnt.transfer_bandwidth_cycles);
}

void measure_bandwidth(void *dst, void *src, size_t size, transfer_test_result_t *result, float frequency) {
    // counts cycles where DMA is busy
    const uint32_t slot_cycles = 0;
    snrt_cfg_perf_counter(slot_cycles, PERF_CNT_DMA_BUSY, 0);
    snrt_reset_perf_counter(slot_cycles);

    // counts bytes written by DMA
    const uint32_t slot_bytes = 1;
    snrt_cfg_perf_counter(slot_bytes, PERF_CNT_DMA_W_BW, 0);
    snrt_reset_perf_counter(slot_bytes);

    snrt_start_perf_counter(slot_cycles);
    snrt_start_perf_counter(slot_bytes);

    uint32_t transfer_start = snrt_mcycle();
    snrt_dma_start_1d(dst, src, size);
    snrt_dma_wait_all();
    uint32_t transfer_end = snrt_mcycle();

    snrt_stop_perf_counter(slot_cycles);
    snrt_stop_perf_counter(slot_bytes);
    
    uint32_t cnt_cycles = snrt_get_perf_counter(slot_cycles);
    uint32_t cnt_bytes = snrt_get_perf_counter(slot_bytes);
    uint32_t sw_cycles = transfer_end - transfer_start;

    result->transfer_bytes = cnt_bytes;
    result->perf_sw.transfer_cycles = sw_cycles;
    result->perf_sw.transfer_bandwidth_cycles = (float)result->transfer_bytes / (float)sw_cycles;
    result->perf_sw.transfer_bandwidth_seconds = result->perf_sw.transfer_bandwidth_cycles * frequency / (1024.f * 1024.f);
    result->perf_cnt.transfer_cycles = cnt_cycles;
    result->perf_cnt.transfer_bandwidth_cycles = (float)result->transfer_bytes / (float)cnt_cycles;
    result->perf_cnt.transfer_bandwidth_seconds = result->perf_cnt.transfer_bandwidth_cycles * frequency / (1024.f * 1024.f);
}

int32_t benchmark_dma_bw(void *args) {
    uint8_t *local_buffer_1_hb0 = 0;
    uint8_t *local_buffer_2_hb0 = 0;
    uint8_t *local_buffer_3_hb1 = 0;

    snrt_init();

    offloadArgs_t *argsStruct = (offloadArgs_t *)args;

    uint32_t core_idx = snrt_cluster_core_idx();

    const int num_tests = 2;
    transfer_test_result_t results[num_tests] = {0};

    if (snrt_is_dm_core()) {
        uint32_t tcdm_base = 0x18000000; // TCDM is 96 KiB
        printf("TCDM (base=%#x, size=%u bytes)\r\n", tcdm_base, SNRT_TCDM_SIZE);

        // first hyperbank
        local_buffer_1_hb0 = snrt_l1_alloc(L2_BUFFER_SIZE);
        // first hyperbank
        // local_buffer_2_hb0 = snrt_l1_alloc(L2_BUFFER_SIZE);
        // second hyperbank
        // local_buffer_3_hb1 = snrt_l1_alloc(L2_BUFFER_SIZE) + SNRT_TCDM_SIZE / 2;
        local_buffer_3_hb1 = (void*)(tcdm_base + L2_BUFFER_SIZE + SNRT_TCDM_SIZE / 2);

        printf("l2_buffer            @ %p -> %p\r\n", &l2_buffer, l2_buffer);
        printf("local_buffer_1_hb0   @ %p -> %p\r\n", &local_buffer_1_hb0, local_buffer_1_hb0);
        printf("local_buffer_3_hb1   @ %p -> %p\r\n", &local_buffer_3_hb1, local_buffer_3_hb1);
        printf("Frequency: %.2f MHz\r\n", argsStruct->frequency / 1e6);

        printf("---------------------------------------\r\n");
        printf("Test: L2 to TCDM\r\n");
        measure_bandwidth(local_buffer_1_hb0, l2_buffer, sizeof(l2_buffer), &results[0], argsStruct->frequency);
        print_test_result(&results[0]);
        
        printf("---------------------------------------\r\n");
        printf("Test: TCDM to TCDM\r\n");
        // measure_bandwidth(local_buffer_3_hb1, local_buffer_1_hb0, sizeof(l2_buffer), &results[1], argsStruct->frequency);
        measure_bandwidth(local_buffer_1_hb0, local_buffer_3_hb1, sizeof(l2_buffer), &results[1], argsStruct->frequency);
        print_test_result(&results[1]);
        
    }

    snrt_cluster_hw_barrier();

    return 0;
}

