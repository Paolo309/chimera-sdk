// Generated register defines for chimera

// Copyright information found in source file:
// Copyright 2024 ETH Zurich and University of Bologna.

// Licensing information found in source file:
//
// SPDX-License-Identifier: SHL-0.51

#ifndef _CHIMERA_REG_DEFS_
#define _CHIMERA_REG_DEFS_

#ifdef __cplusplus
extern "C" {
#endif
// Register width
#define CHIMERA_PARAM_REG_WIDTH 32

// Set boot address for all snitch cores
#define CHIMERA_SNITCH_BOOT_ADDR_REG_OFFSET 0x0

// Define the address of the Boot executed by each Snitch core
#define CHIMERA_SNITCH_CONFIGURABLE_BOOT_ADDR_REG_OFFSET 0x4

// Set interrupt handler address for all snitch cores
#define CHIMERA_SNITCH_INTR_HANDLER_ADDR_REG_OFFSET 0x8

// Register to store return value of Snitch cluster 0
#define CHIMERA_SNITCH_CLUSTER_0_RETURN_REG_OFFSET 0xc

// Register to store return value of Snitch cluster 1
#define CHIMERA_SNITCH_CLUSTER_1_RETURN_REG_OFFSET 0x10

static const uint8_t _chimera_snitch_return_reg_offset[] = {
    CHIMERA_SNITCH_CLUSTER_0_RETURN_REG_OFFSET,
    CHIMERA_SNITCH_CLUSTER_1_RETURN_REG_OFFSET
};

// Soft reset for cluster 0. Active High
#define CHIMERA_RESET_CLUSTER_0_REG_OFFSET 0x20
#define CHIMERA_RESET_CLUSTER_0_RESET_CLUSTER_0_BIT 0

// Soft reset for cluster 1. Active High
#define CHIMERA_RESET_CLUSTER_1_REG_OFFSET 0x24
#define CHIMERA_RESET_CLUSTER_1_RESET_CLUSTER_1_BIT 0

static const uint8_t _chimera_reset_reg_offset[] = {
    CHIMERA_RESET_CLUSTER_0_REG_OFFSET,
    CHIMERA_RESET_CLUSTER_1_REG_OFFSET
};

static const uint8_t _chimera_reset_bit[] = {
    CHIMERA_RESET_CLUSTER_0_RESET_CLUSTER_0_BIT,
    CHIMERA_RESET_CLUSTER_1_RESET_CLUSTER_1_BIT
};

// Enable clock gate for cluster 0
#define CHIMERA_CLUSTER_0_CLK_GATE_EN_REG_OFFSET 0x34
#define CHIMERA_CLUSTER_0_CLK_GATE_EN_CLUSTER_0_CLK_GATE_EN_BIT 0

// Enable clock gate for cluster 1
#define CHIMERA_CLUSTER_1_CLK_GATE_EN_REG_OFFSET 0x38
#define CHIMERA_CLUSTER_1_CLK_GATE_EN_CLUSTER_1_CLK_GATE_EN_BIT 0

static const uint8_t _chimera_clk_gate_en_reg_offset[] = {
    CHIMERA_CLUSTER_0_CLK_GATE_EN_REG_OFFSET,
    CHIMERA_CLUSTER_1_CLK_GATE_EN_REG_OFFSET
};

static const uint8_t _chimera_clk_gate_en_bit[] = {
    CHIMERA_CLUSTER_0_CLK_GATE_EN_CLUSTER_0_CLK_GATE_EN_BIT,
    CHIMERA_CLUSTER_1_CLK_GATE_EN_CLUSTER_1_CLK_GATE_EN_BIT
};

// Bypass cluster to mem wide connection for cluster 0
#define CHIMERA_WIDE_MEM_CLUSTER_0_BYPASS_REG_OFFSET 0x48
#define CHIMERA_WIDE_MEM_CLUSTER_0_BYPASS_WIDE_MEM_CLUSTER_0_BYPASS_BIT 0

// Bypass cluster to mem wide connection for cluster 1
#define CHIMERA_WIDE_MEM_CLUSTER_1_BYPASS_REG_OFFSET 0x4c
#define CHIMERA_WIDE_MEM_CLUSTER_1_BYPASS_WIDE_MEM_CLUSTER_1_BYPASS_BIT 0

static const uint8_t _chimera_wide_mem_bypass_reg_offset[] = {
    CHIMERA_WIDE_MEM_CLUSTER_0_BYPASS_REG_OFFSET,
    CHIMERA_WIDE_MEM_CLUSTER_1_BYPASS_REG_OFFSET
};

static const uint8_t _chimera_wide_mem_bypass_bit[] = {
    CHIMERA_WIDE_MEM_CLUSTER_0_BYPASS_WIDE_MEM_CLUSTER_0_BYPASS_BIT,
    CHIMERA_WIDE_MEM_CLUSTER_1_BYPASS_WIDE_MEM_CLUSTER_1_BYPASS_BIT
};

// Register to identify when cluster 0 is busy
#define CHIMERA_CLUSTER_0_BUSY_REG_OFFSET 0x5c
#define CHIMERA_CLUSTER_0_BUSY_CLUSTER_0_BUSY_BIT 0

// Register to identify when cluster 1 is busy
#define CHIMERA_CLUSTER_1_BUSY_REG_OFFSET 0x60
#define CHIMERA_CLUSTER_1_BUSY_CLUSTER_1_BUSY_BIT 0

static const uint8_t _chimera_busy_reg_offset[] = {
    CHIMERA_CLUSTER_0_BUSY_REG_OFFSET,
    CHIMERA_CLUSTER_1_BUSY_REG_OFFSET
};

static const uint8_t _chimera_busy_bit[] = {
    CHIMERA_CLUSTER_0_BUSY_CLUSTER_0_BUSY_BIT,
    CHIMERA_CLUSTER_1_BUSY_CLUSTER_1_BUSY_BIT
};

#ifdef __cplusplus
} // extern "C"
#endif
#endif // _CHIMERA_REG_DEFS_
       // End generated register defines for chimera