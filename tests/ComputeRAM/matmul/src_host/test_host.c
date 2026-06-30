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

#include "patch_embed_matmul.h"

// volatile uint64_t * CXR_BASE_ADDR = (volatile uint64_t *)0x40A00000;

// void prepare_patch_embed_matmul(void) __attribute__((weak));
// void patch_embed_matmul(void *input, void *output);

extern void prepare_patch_embed_matmul(void);
extern void patch_embed_matmul(const int8_t *input, int32_t *output);

// int patch_embed_matmul(int a, int b);

// #define N_PATCHES 196
// #define IN_DIM    768
// #define OUT_DIM   192
// #define N_INPUTS  4

// #define N_PATCHES PATCH_EMBED_M
// #define IN_DIM    PATCH_EMBED_K
// #define OUT_DIM   PATCH_EMBED_N
// #define N_INPUTS  1

// #define NUM_BYTES 32 * 1024
// uint64_t buffer[NUM_BYTES / 8];

int main(void) {
    // static int8_t  inputs[N_INPUTS][N_PATCHES * IN_DIM];  /* a few images' patches   */
    static int32_t output[MAT_N * MAT_M];           /* one forward pass' result */

    printf("Starting patch_embed_matmul test\n");
    memset(output, 0, sizeof(output));

    printf("Preparing patch_embed_matmul\n");
    prepare_patch_embed_matmul();

    printf("Running patch_embed_matmul\n");
    
    uint32_t start_cycle = get_mcycle();
    patch_embed_matmul(&patch_embed_act[0][0], output);
    uint32_t read_end_cycle = get_mcycle();
    
    printf("Finished patch_embed_matmul\n");
    printf("Execution cycles: %d\n", read_end_cycle - start_cycle);

    int errors = 0;
    for (int i = 0; i < MAT_N * MAT_M; i++) {
        int32_t golden = patch_embed_golden[i / MAT_M][i % MAT_M];
        if (output[i] != golden) {
            printf("Mismatch at index %d: output = %d, golden = %d\n", i, output[i], golden);
            errors++;
        }
    }

    printf("[%s] Test completed with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    return 0;
}
