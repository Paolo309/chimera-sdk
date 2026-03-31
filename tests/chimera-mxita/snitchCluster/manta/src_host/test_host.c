// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

// Include Standard Libraries

// Include Application Headers
#include "test_cluster.h"
#include "test_host.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "driver.h"

// Include Runtime Headers
#include "log.h"

#include "math.h"

// Import HAL Headers
#include "interface_api.h"
#include "util.h"


// ---- Clusters' stack configuration and reset ----

// TODO print both TCDM size and selected stack size
#define STACK_ADDRESS(idx) (_chimera_clusterBase[(idx)] + 0x18000 - 1)
#define STACK_SIZE 0x1000

void *stack_cluster_0_ptr[CLUSTER_0_NUMCORES];
void *stack_cluster_1_ptr[CLUSTER_1_NUMCORES];
void **stack_cluster_ptr[] = {stack_cluster_0_ptr, stack_cluster_1_ptr};

/**
 * @brief Resets the specified cluster.
 *
 * @param cluster_id The ID of the cluster to reset.
*/
void reset_cluster(int cluster_id) {
    printf_log("reset cluster %d \r\n", cluster_id);

    reset_snitchCluster_busy(cluster_id);
    set_snitchCluster_clockGating(cluster_id, 0);
    set_snitchCluster_reset(cluster_id, 1);
    for (volatile int i = 0; i < 10; i++);
    set_snitchCluster_reset(cluster_id, 0);
    set_snitchCluster_clockGating(cluster_id, 1);
}


// ---- Cluster syscall communication ----

extern uintptr_t volatile tohost, fromhost;
void handle_cluster_syscalls(int cluster_id);


// ---- Test Offloading and Interrupt Handling ----

static offloadArgs_t offloadArgs = {0};

uint32_t cluster_run(
    int cluster_idx,
    int32_t (*cluster_test_fn)(void*)
) {
    // setup_snitchCluster_interruptHandler(cluster_test_interrupt_handler);
    setup_snitchCluster_interruptHandler(clusterInterruptHandler);

    set_snitchCluster_clockGating(cluster_idx, 0);

    offload_snitchCluster(cluster_test_fn, &offloadArgs, stack_cluster_ptr[cluster_idx], cluster_idx);

    // Handle tohost/fromhost communication, returns when cluster is done
    handle_cluster_syscalls(cluster_idx);

    uint32_t retVal = wait_snitchCluster_return(cluster_idx);
    set_snitchCluster_clockGating(cluster_idx, 1);

    return retVal >> 1;
}

int test_default_fp32() {
    offloadArgs.bf16_sel = 1; // FP32
    return cluster_run(
        0, /* cluster idx */
        test_mxita
    );
}


int run_mxita_tests() {
    const uint32_t NUM_RUNS = 1;
    offloadArgs.hw_cycles = 0;
    offloadArgs.sw_cycles = 0;
    offloadArgs.bf16_sel = 1;
    offloadArgs.run_concurrent_tcdm = 0;

    uint32_t rtc_freq = *(uint32_t*)reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    offloadArgs.frequency = clint_get_core_freq(rtc_freq, 512);

    int final_ret = 0;

    printf_log("Running default FP32 test for %d runs\r\n", NUM_RUNS);
    for (int run_idx = 0; run_idx < NUM_RUNS; run_idx++) {
        printf_log("\r\n---- Sub-run %d/%d ----\r\n", run_idx + 1, NUM_RUNS);
        final_ret |= cluster_run(
            0, /* cluster idx */
            test_mxita
        );
    }

    uint32_t avg_hw_cycles = (offloadArgs.hw_cycles * 1000U) / NUM_RUNS;
    uint32_t avg_sw_cycles = (offloadArgs.sw_cycles * 1000U) / NUM_RUNS;

    printf_log("\r\n=== Performance Summary ===\r\n");
    printf_log("  Number of runs:    %d\r\n", NUM_RUNS);
    printf_log("  Average HW Cycles: %u.%u\r\n", avg_hw_cycles / 1000U, avg_hw_cycles % 1000U);
    printf_log("  Average SW Cycles: %u.%u\r\n", avg_sw_cycles / 1000U, avg_sw_cycles % 1000U);

    return final_ret;
}

test_entry_t benchmarks[] = {
    {"frep", benchmark_frep},
    {"freb4d_4", benchmark_freb4d_4},
    {"gemm_frep", benchmark_gemm_frep},
    {"DMA BW", benchmark_dma_bw},
};

int run_benchmarks() {
    int failed = 0;
    
    const int NUM_BENCH = sizeof(benchmarks) / sizeof(benchmarks[0]);

    uint32_t rtc_freq = *(uint32_t*)reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    offloadArgs.frequency = clint_get_core_freq(rtc_freq, 512);

    for (int i = 0; i < NUM_BENCH; i++) {
        printf_log("\r\n---- Running benchmark: %s ----\r\n", benchmarks[i].name);
        int ret = cluster_run(
            0, /* cluster idx */
            benchmarks[i].fn
        );
        if (ret != 0) {
            printf_log("Benchmark %s FAILED with return value %d\r\n", benchmarks[i].name, ret);
            failed += 1;
        } else {
            printf_log("Benchmark %s PASSED\r\n", benchmarks[i].name);
        }
    }

    return failed;
}

test_entry_t kernels[] = {
    // {"matmul", matmul_test},
    // {"vexpf", snitch_app_0},
    // {"transfer_test", benchmark_dma_bw},
    {"gemm", kernel_gemm},
    {"vexpf", kernel_vexpf},
};

int run_kernels() {
    int failed = 0;
    
    uint32_t rtc_freq = *(uint32_t*)reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
    offloadArgs.frequency = clint_get_core_freq(rtc_freq, 512);

    const int NUM_KERNELS = sizeof(kernels) / sizeof(kernels[0]);

    for (int i = 0; i < NUM_KERNELS; i++) {
        printf_log("\r\n---- Running kernel: %s ----\r\n", kernels[i].name);
        int ret = cluster_run(
            0, /* cluster idx */
            kernels[i].fn
        );
        if (ret != 0) {
            printf_log("Kernel %s FAILED with return value %d\r\n", kernels[i].name, ret);
            failed += 1;
        } else {
            printf_log("Kernel %s PASSED\r\n", kernels[i].name);
        }
    }

    return failed;
}


test_entry_t tests[] = {
    {"Kernels", run_kernels},
    {"MXITA tests", run_mxita_tests},
    {"Benchmarks", run_benchmarks},
};
const int NUM_TESTS = sizeof(tests) / sizeof(tests[0]);

int run_test(int test_idx) {
    printf_log("==================== TEST %2d ====================\r\n", test_idx);
    printf_log("Name      : %s\r\n", tests[test_idx].name);
    printf_log("Cluster   : running...\r\n");
    printf_log("-------------------------------------------------\r\n");
    uint32_t retVal = tests[test_idx].fn();
    printf_log("-------------------------------------------------\r\n");
    printf_log("Result    : [%s] (return=%d)\r\n", retVal == 0 ? "PASS" : "FAIL", retVal);
    printf_log("===============================================\r\n\r\n");
    return retVal;
}

int main() {
    printf("=== MXITA Test @ " BACKEND_NAME " ===\r\n");

    for (int cluster_idx = 0; cluster_idx < _chimera_numClusters; cluster_idx++) {
        generate_snitchCluster_SPs_uniform(cluster_idx, (void *)STACK_ADDRESS(cluster_idx), STACK_SIZE,
                                           stack_cluster_ptr[cluster_idx]);

        reset_cluster(cluster_idx);
    }

#if defined(HARDWARE_BACKEND_RTL)
    offloadArgs.is_rtl = 1;
#endif

    uint32_t failed_tests = 0;

    for (int test_idx = 0; test_idx < NUM_TESTS; test_idx++) {
        failed_tests += (run_test(test_idx) != 0);
    }

    printf_log("MXITA Test Summary: %d/%d tests passed, %d failed.\r\n\r\n", NUM_TESTS - failed_tests,
               NUM_TESTS, failed_tests);

    return failed_tests;
}

void handle_cluster_syscalls(int cluster_id) {
    while (snitchCluster_busy(cluster_id)) {
        // Wait for tohost to be set by the device
        if (tohost != 0) {
            volatile uint32_t syscall_addr = tohost;

            // Acknowledge tohost
            tohost = 0;

            // printf("Host received tohost: %#x\r\n", tohost);

            // Cluster does tohost = (uintptr_t)buf->hdr.syscall_mem;
            uint32_t *syscall_mem = (uint32_t *)syscall_addr;

            // printf("Host handling syscall %u: fd=%#x, buf=%p, len=%#x\r\n", syscall_mem[0],
            //        syscall_mem[1], (void *)syscall_mem[2], syscall_mem[3]);
            if (syscall_mem[0] == 64) { // sys_write
                fwrite((const void *)syscall_mem[2], 1, syscall_mem[3], (FILE *)syscall_mem[1]);
                fflush((FILE *)syscall_mem[1]);
                // printf_log("handled syscall: %u\r\n", syscall_mem[0]);
            } else {
                printf_log("Unknown syscall: %u\r\n", syscall_mem[0]);
            }

            // Notify cluster that syscall is done
            fromhost = syscall_addr;
        }
    }
}
