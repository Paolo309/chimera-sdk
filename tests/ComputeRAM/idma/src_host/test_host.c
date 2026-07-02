// SPDX-FileCopyrightText: 2024 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>

#include "soc.h"
#include "util.h"
#include "uart.h"
#include "dif/dma.h"

#include "driver.h"
#include "log.h"
#include "shared.h"

#define TRANSFER_SIZE 128

// static volatile uint32_t L2_BUFFER[TRANSFER_SIZE];
// volatile uint32_t *L2_BUFFER = (volatile uint32_t *)0x10000000UL; // SPM
volatile uint32_t *L2_BUFFER = (volatile uint32_t *)0x14000000UL; // SPM UNCACHED

#define CXR_OFFSET 0x10000
#define NUM_CXR 8
volatile uint64_t* CXR_BASE = (volatile uint64_t *)0x40A00000UL;

volatile uint32_t * cxr_mcast_mask_lo = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_MULTICAST_MASK_LO_REG_OFFSET);
volatile uint32_t * cxr_mcast_mask_hi = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_MULTICAST_MASK_HI_REG_OFFSET);

uint64_t* get_cxr_base(int cxr_index) {
    if (cxr_index < 0 || cxr_index >= NUM_CXR) {
        printf("Error: CxR index %d is out of bounds (0-%d)\n", cxr_index, NUM_CXR - 1);
        return NULL;
    }
    return (volatile uint64_t *)((uintptr_t)CXR_BASE + (cxr_index * CXR_OFFSET));
}

void enable_cxr_multicast() {
    uint64_t cxr_multicast_mask = 0;
    for (int i = 0; i < NUM_CXR; i++) {
        cxr_multicast_mask |= (uint64_t)i * CXR_OFFSET;
    }

    *cxr_mcast_mask_lo = (uint32_t)(cxr_multicast_mask & 0xFFFFFFFF);
    *cxr_mcast_mask_hi = (uint32_t)((cxr_multicast_mask >> 32) & 0xFFFFFFFF);

    fence();
}

void disable_cxr_multicast() {
    *cxr_mcast_mask_lo = 0x00000000;
    *cxr_mcast_mask_hi = 0x00000000;
    fence();
}

void init_mem(uint32_t *buf, size_t size) {
    for (int i = 0; i < size; i++) {
        buf[i] = (uint32_t)i;
    }
}

void fill32(volatile uint32_t *p, uint32_t value, size_t n) {
    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }
    fence();
}

void test_transfer(uint32_t *dst, uint32_t *src, size_t size) {
    printf("Initializing source buffer (0x%08x)...\n", (uint32_t)(uintptr_t)src);
    init_mem(src, size);
    fence();
    printf("Clearing destination buffer (0x%08x)...\n", (uint32_t)(uintptr_t)dst);
    fill32(dst, 0xFF, size);
    fence();

    printf("Starting DMA transfer of %zu bytes from 0x%08x to 0x%08x...\n", size * sizeof(uint32_t), (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst);
    sys_dma_blk_memcpy(
        (uintptr_t)dst, 
        (uintptr_t)src, 
        size * sizeof(uint32_t),
        DMA_CONF_DECOUPLE_NONE
    );

    printf("DMA transfer completed. Verifying destination buffer...\n");
}

int test_transfer_unicast(uint32_t *dst, uint32_t *src, size_t size) {
    test_transfer(dst, src, size);

    int errors = 0;
    for (int i = 0; i < size; i++) {
        if (dst[i] != (uint32_t)i) {
            printf("Mismatch at index %d: got 0x%08x, expected 0x%08x\n", i, dst[i], (uint32_t)i);
            errors++;
        }
    }
    return errors;
}

int test_transfer_mcast(uint32_t *dst, uint32_t *src, size_t size) {
    test_transfer(dst, src, size);

    // Verify that all CxRs received the same data
    int errors = 0;
    for (int cxr = 0; cxr < NUM_CXR; cxr++) {
        volatile uint32_t *curr_cxr = (volatile uint32_t *)get_cxr_base(cxr);
        for (int i = 0; i < size; i++) {
            if (curr_cxr[i] != (uint32_t)i) {
                printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr, i, curr_cxr[i], (uint32_t)i);
                errors++;
            }
        }
    }

    return errors;
}

int main(void) {
    printf("--- CxR iDMA/multicast test ---\n");

    printf("Test 1: unicast iDMA write to CxR[0]\n");
    int errors = test_transfer_unicast(get_cxr_base(0), L2_BUFFER, TRANSFER_SIZE);
    printf("[%s] Test 1 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    printf("Test 2: unicast iDMA read from CxR[0]\n");
    errors = test_transfer_unicast(L2_BUFFER, get_cxr_base(0), TRANSFER_SIZE);
    printf("[%s] Test 2 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    printf("Enabling CxR multicast...\n");
    enable_cxr_multicast();

    printf("Test 3: multicast iDMA write to CxR[0-7]\n");
    errors = test_transfer_mcast(get_cxr_base(0), L2_BUFFER, TRANSFER_SIZE);
    printf("[%s] Test 3 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    printf("Test 4: multicast iDMA read from CxR[0-7]\n");
    errors = test_transfer_mcast(L2_BUFFER, get_cxr_base(0), TRANSFER_SIZE);
    printf("[%s] Test 4 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    disable_cxr_multicast();

    return errors != 0;
}
