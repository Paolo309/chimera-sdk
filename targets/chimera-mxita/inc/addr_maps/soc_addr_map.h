// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

#ifndef _SOC_ADDR_MAP_INCLUDE_GUARD_
#define _SOC_ADDR_MAP_INCLUDE_GUARD_

#include <stdint.h>

#define CLINT_CTRL_BASE 0x02040000

#define SOC_CTRL_BASE 0x30001000

#define CLUSTER_0_BASE 0x40000000
#define CLUSTER_1_BASE 0x40200000

#define HOST_NUMCORES 1
#define CLUSTER_0_NUMCORES 5
#define CLUSTER_1_NUMCORES 5

#define NUM_CLUSTER_CORES (CLUSTER_0_NUMCORES + CLUSTER_1_NUMCORES)

#define NUM_CORES (HOST_NUMCORES + NUM_CLUSTER_CORES)

#define HOST_HART_BASE 0
#define CLUSTER_0_HART_BASE (HOST_HART_BASE + HOST_NUMCORES)
#define CLUSTER_1_HART_BASE (CLUSTER_0_HART_BASE + CLUSTER_0_NUMCORES)

#define CLUSTER_HART_BASE CLUSTER_0_HART_BASE

static const uint8_t _chimera_numCores[] = {CLUSTER_0_NUMCORES, CLUSTER_1_NUMCORES};
static const uint8_t _chimera_hartBase[] = {CLUSTER_0_HART_BASE, CLUSTER_1_HART_BASE};

static const uint32_t _chimera_clusterBase[] = {CLUSTER_0_BASE, CLUSTER_1_BASE};

extern volatile uint32_t __l1_c0_heap_start, __l1_c1_heap_start;
static const uint32_t _chimera_clusterHeapStart[] = {(uint32_t)&__l1_c0_heap_start,
                                                     (uint32_t)&__l1_c1_heap_start};

extern volatile uint32_t __l1_c0_start, __l1_c1_start, __l1_c2_start;
static const uint32_t _chimera_clusterL1Start[] = {(uint32_t)&__l1_c0_start,
                                                   (uint32_t)&__l1_c1_start};

extern volatile uint32_t __l1_c0_end, __l1_c1_end;
static const uint32_t _chimera_clusterL1End[] = {(uint32_t)&__l1_c0_end, (uint32_t)&__l1_c1_end};

extern volatile uint32_t __l1_c0_lma_start, __l1_c1_lma_start;
static const uint32_t _chimera_clusterL1LmaStart[] = {(uint32_t)&__l1_c0_lma_start,
                                                      (uint32_t)&__l1_c1_lma_start};

extern volatile uint32_t __l1_c0_lma_end, __l1_c1_lma_end;
static const uint32_t _chimera_clusterL1LmaEnd[] = {(uint32_t)&__l1_c0_lma_end,
                                                    (uint32_t)&__l1_c1_lma_end};

#define _chimera_numClusters 2

#define CHIMERA_PADFRAME_BASE_ADDRESS 0x30002000
#define FLL_BASE_ADDR 0x30003000

#endif
