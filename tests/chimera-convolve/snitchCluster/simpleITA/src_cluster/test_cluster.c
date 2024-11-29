// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Moritz Scherer <scheremo@iis.ee.ethz.ch>

#include "test_cluster.h"
#include "test_host.h"
#include "ita.h"

#include "soc.h"
#include "cluster_4.h"

static uint32_t *clintPointer = (uint32_t *)CLINT_CTRL_BASE;

/**
 * @brief Interrupt handler for the cluster, which clears the interrupt flag for the current hart.
 *
 * @warning Stack, thread and global pointer might not yet be set up!
 */
__attribute__((naked)) void clusterInterruptHandler() {
    asm volatile(
        // Load global pointer
        ".option push\n"
        ".option norelax\n"          // Disable relaxation to ensure `la` behaves as expected
        "la gp, __global_pointer$\n" // Load address of global pointer
        ".option pop\n"

        // Set thread pointer (tp) to zero
        "mv tp, zero\n"

        // Load mhartid CSR into t0
        "csrr t0, mhartid\n"
        // Load the base address of clintPointer into t1
        "lw t1, %0\n"
        // Calculate the interrupt target address: t1 = t1 + (t0 * 4)
        "slli t0, t0, 2\n"
        "add t1, t1, t0\n"
        // Store 0 to the interrupt target address
        "sw zero, 0(t1)\n"
        "ret"
        :
        : "m"(clintPointer) // Pass clintPointer as input
        : "t0", "t1"        // Declare clobbered registers
    );
}

const uint32_t requant_eps_mult[3][2] = {
    {1751606885, 27241}, // Packed values [101 102 103 104 105 106]
    {1920036975, 29811}, // Packed values [111 112 113 114 115 116]
    {2088467065, 32381}  // Packed values [121 122 123 124 125 126]
};
const uint32_t requant_right_shift[3][2] = {
    {3604337875, 55511}, // Packed values [211 212 213 214 215 216]
    {3772767965, 58081}, // Packed values [221 222 223 224 225 226]
    {3941198055, 60651}  // Packed values [231 232 233 234 235 236]
};
const uint32_t requant_add[3][2] = {
    {67305985, 1541},  // Packed values [1 2 3 4 5 6]
    {235736075, 4111}, // Packed values [11 12 13 14 15 16]
    {404166165, 6681}  // Packed values [21 22 23 24 25 26]
};

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

    ita_soft_clear();
    ita_acquire_job();

    // Programm the first context
    ita_write_rqs_params(requant_eps_mult[0][0], requant_eps_mult[0][1], requant_right_shift[0][0],
                         requant_right_shift[0][1], requant_add[0][0], requant_add[0][1]);
    ita_write_regs((uint32_t)CLUSTER_4_TCDM_START_ADDR, CLUSTER_4_TCDM_START_ADDR + 0x1000,
                   CLUSTER_4_TCDM_START_ADDR + 0x2000, CLUSTER_4_TCDM_START_ADDR + 0x3000,
                   CLUSTER_4_TCDM_START_ADDR + 0x4000, (uint32_t)ITA_TILES(1, 1, 1),
                   (uint32_t)ITA_LAYER(ATTENTION, IDENTITY), (uint32_t)ITA_FLAGS(1, 1, 0, 0, 0));

    // Commit the first context into the queue
    ita_commit();

    // Program second context
    ita_write_rqs_params(requant_eps_mult[1][0], requant_eps_mult[1][1], requant_right_shift[1][0],
                         requant_right_shift[1][1], requant_add[1][0], requant_add[1][1]);
    ita_write_regs(
        (uint32_t)CLUSTER_4_TCDM_START_ADDR + 0x100, CLUSTER_4_TCDM_START_ADDR + 0x1000 + 0x100,
        CLUSTER_4_TCDM_START_ADDR + 0x2000 + 0x100, CLUSTER_4_TCDM_START_ADDR + 0x3000 + 0x100,
        CLUSTER_4_TCDM_START_ADDR + 0x4000 + 0x100, (uint32_t)ITA_TILES(1, 1, 1),
        (uint32_t)ITA_LAYER(ATTENTION, IDENTITY), (uint32_t)ITA_FLAGS(0, 1, 0, 0, 0));

    // Commit the second context into the queue and start the execution
    ita_trigger();

    // Make sure accelerator can get new job into the queue
    ita_acquire_job();

    // Programm the third context
    ita_write_rqs_params(requant_eps_mult[2][0], requant_eps_mult[2][1], requant_right_shift[2][0],
                         requant_right_shift[2][1], requant_add[2][0], requant_add[2][1]);
    ita_write_regs(
        (uint32_t)CLUSTER_4_TCDM_START_ADDR + 0x200, CLUSTER_4_TCDM_START_ADDR + 0x1000 + 0x200,
        CLUSTER_4_TCDM_START_ADDR + 0x2000 + 0x200, CLUSTER_4_TCDM_START_ADDR + 0x3000 + 0x200,
        CLUSTER_4_TCDM_START_ADDR + 0x4000 + 0x200, (uint32_t)ITA_TILES(1, 1, 1),
        (uint32_t)ITA_LAYER(ATTENTION, IDENTITY), (uint32_t)ITA_FLAGS(0, 0, 0, 0, 0));

    // Commit the third context into the queue and start the execution if not already running
    ita_trigger();

    // Wait for final result
    ita_wait_job();

    return TESTVAL;
}
