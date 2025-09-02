// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Philip Wiese <wiesep@iis.ee.ethz.ch>

#ifndef _ITA_INCLUDE_GUARD_
#define _ITA_INCLUDE_GUARD_

#include "soc.h"
#include "cluster_4.h"

#include <stdint.h>

// WIESEP: This part should go into a cluster specific SDK

// ITA Accelerator
typedef struct __attribute__((__packed__)) {
    int value : 24;
} ita_int24_t;

typedef enum {
    ATTENTION = 0x0,   // 00 in binary
    FEEDFORWARD = 0x1, // 01 in binary
    LINEAR = 0x2       // 10 in binary
} LayerType;

typedef enum {
    IDENTITY = 0x0, // 00 in binary
    GELU = 0x1,     // 01 in binary
    RELU = 0x2      // 10 in binary
} ActivationType;

#define ITA_TILES(s, e, p) (s | (e << 4) | (p << 8))
#define ITA_LAYER(layer, activation) ((layer) | ((activation) << 2))
#define ITA_FLAGS(weight_preload, weight_nextload, bias_disable, bias_direction, output_disable) \
    ((weight_preload) | ((weight_nextload) << 1) | ((bias_disable) << 2) | \
     ((bias_direction) << 3) | ((output_disable) << 4))

static inline void __attribute((always_inline))
ita_write_regs(uint32_t input_addr, uint32_t weight_addr, uint32_t weight_next_addr,
               uint32_t bias_addr, uint32_t output_addr, uint32_t tiles, uint32_t layer,
               uint32_t flags) {
    // Program ITA
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x20) = input_addr - CLUSTER_4_BASE;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x24) = weight_addr - CLUSTER_4_BASE;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x28) = weight_next_addr - CLUSTER_4_BASE;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x2C) = bias_addr - CLUSTER_4_BASE;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x30) = output_addr - CLUSTER_4_BASE;
    // unused sequence length
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x38) = tiles;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x54) = layer; // ctrl engine
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x58) = flags; // ctrl stream
}

static inline void __attribute((always_inline))
ita_write_rqs_params(uint32_t eps1, uint32_t eps2, uint32_t right_shift1, uint32_t right_shift2,
                     uint32_t add1, uint32_t add2) {
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x3C) = eps1;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x40) = eps2;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x44) = right_shift1;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x48) = right_shift2;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x4C) = add1;
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x50) = add2;
}

static inline void __attribute((always_inline)) ita_soft_clear() {
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x14) = 0;
    for (volatile int i = 0; i < 10; i++)
        ;
}

static inline void __attribute((always_inline)) ita_soft_clear_keep_regs() {
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x14) = 1;
    for (volatile uint32_t i = 0; i < 10; i++)
        ;
}

static inline void __attribute((always_inline)) ita_acquire_job() {
    while (*(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x04) < 1)
        ;
}

static inline void __attribute((always_inline)) ita_wait_job() {
    while (*(volatile uint32_t *)(CLUSTER_4_HWPE_BUSY_ADDR) != 0)
        ;
}

static inline void __attribute((always_inline)) ita_trigger() {
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x00) = 0;
}

static inline void __attribute((always_inline)) ita_commit() {
    *(volatile uint32_t *)(CLUSTER_4_HWPE_ITA_BASE_ADDR + 0x00) = 1;
}

#endif //_ITA_INCLUDE_GUARD_

