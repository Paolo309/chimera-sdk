// SPDX-FileCopyrightText: 2024 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

// Include Standard Libraries
#include <stdio.h>
#include <string.h>

// Include Application Headers

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "driver.h"

// Include Runtime Headers
#include "util.h"

// Import HAL Headers
#include "clint.h"
#include "uart.h"

#define CXR_TYPE uint64_t
#define NUM_CXR 8
#define CXR_OFFSET 0x10000
volatile CXR_TYPE * CXR_BASE_ADDR = (volatile CXR_TYPE *)0x40A00000;

#define BUFFER_SIZE 8*128 // bytes
#define BUFFER_TYPE int64_t
#define BUFFER_LENGTH (BUFFER_SIZE / sizeof(BUFFER_TYPE))
static BUFFER_TYPE input_buffer[BUFFER_LENGTH];

int main(void) {
    printf("initializing buffer...\n");
    for (BUFFER_TYPE i = 0; i < BUFFER_LENGTH; i++) {
        input_buffer[i] = i;
    }

    volatile int32_t * cxr_mcast_mask_lo = (volatile int32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_MULTICAST_MASK_LO_REG_OFFSET);
    volatile int32_t * cxr_mcast_mask_hi = (volatile int32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_MULTICAST_MASK_HI_REG_OFFSET);

    *cxr_mcast_mask_lo = 0x00070000;
    *cxr_mcast_mask_hi = 0x00000000;

    printf("writing (broadcast) buffer to CXR_BASE_ADDR...\n");
    for (BUFFER_TYPE i = 0; i < BUFFER_LENGTH; i++) {
        CXR_BASE_ADDR[i] = input_buffer[i];
    }

    *cxr_mcast_mask_lo = 0x00000000;
    *cxr_mcast_mask_hi = 0x00000000;

    printf("reading buffer from CXR_BASE_ADDR...\n");
    int errors = 0;
    for (int cxr = 0; cxr < NUM_CXR; cxr++) {
        volatile CXR_TYPE *curr_cxr = CXR_BASE_ADDR + (cxr * CXR_OFFSET / sizeof(CXR_TYPE));
        printf("CxR %d, address: %p\n", cxr, curr_cxr);
        for (BUFFER_TYPE i = 0; i < BUFFER_LENGTH; i++) {
            BUFFER_TYPE cxr_val = curr_cxr[i];
            if (cxr_val != input_buffer[i]) {
                printf("Error: CxR %d, address %p, CXR_BASE_ADDR[%d] = %d, input_buffer[%d] = %d\n", cxr, curr_cxr, i, cxr_val, i, input_buffer[i]);
                errors++;
            }
        }
    }

    printf("[%s] %d errors found.\n", errors == 0 ? "PASS" : "FAIL", errors);

    return errors != 0;
}
