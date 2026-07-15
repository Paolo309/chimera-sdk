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

#define TRANSFER_SIZE 512

// static volatile uint32_t L2_BUFFER[TRANSFER_SIZE];
// volatile uint32_t *L2_BUFFER = (volatile uint32_t *)0x10000000UL; // SPM
volatile uint32_t *L2_BUFFER = (volatile uint32_t *)0x14000000UL; // SPM UNCACHED

// #define CXR_OFFSET 0x10000
#define CXR_OFFSET 0x8000
#define NUM_CXR 8
#define CXR_LOCAL_BYTE_ADDR_WIDTH 16
volatile uint64_t* CXR_BASE = (volatile uint64_t *)0x40A00000UL;

volatile uint32_t * cxr_mcast_mask_lo = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_MULTICAST_MASK_LO_REG_OFFSET);
volatile uint32_t * cxr_mcast_mask_hi = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_MULTICAST_MASK_HI_REG_OFFSET);

volatile uint32_t* cxr_config = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_CONFIG_REG_OFFSET);

// CxR DMA performance monitor registers
volatile uint32_t* cxr_perf_ctrl    = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_PERF_CTRL_REG_OFFSET);
volatile uint32_t* cxr_perf_dur_ar  = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_PERF_DUR_AR_REG_OFFSET);
volatile uint32_t* cxr_perf_dur_r   = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_PERF_DUR_R_REG_OFFSET);
volatile uint32_t* cxr_perf_beats   = (volatile uint32_t *)(SOC_CTRL_BASE + CHIMERA_CXR_PERF_BEATS_REG_OFFSET);

static inline void cxr_perf_arm() {
    *cxr_perf_ctrl = 1;
    fence();
}

static inline void cxr_perf_stop() {
    *cxr_perf_ctrl = 0;
    fence();
}

// uint64_t* get_cxr_base(int cxr_index) {
//     if (cxr_index < 0 || cxr_index >= NUM_CXR) {
//         printf("Error: CxR index %d is out of bounds (0-%d)\n", cxr_index, NUM_CXR - 1);
//         return NULL;
//     }
//     return (volatile uint64_t *)((uintptr_t)CXR_BASE + (cxr_index * CXR_OFFSET));
// }
inline uint64_t* get_cxr_base(int cxr_index) {
    return (volatile uint64_t *)((uintptr_t)CXR_BASE + (cxr_index * CXR_OFFSET));
}

void enable_cxr_multicast(int num_cxr) {
    uint64_t cxr_multicast_mask = 0;
    
    // For Contigous-Bank mapping
    // for (int i = 0; i < NUM_CXR; i++) {
    //     cxr_multicast_mask |= (uint64_t)i * CXR_OFFSET;
    // }

    // For Contigous-Type mapping
    for (int i = 0; i < num_cxr; i++) {
        cxr_multicast_mask |= ((uint64_t)i << CXR_LOCAL_BYTE_ADDR_WIDTH);
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

void enable_cxr_interleaved() {
    *cxr_config |= (1u << CHIMERA_CXR_CONFIG_INTERLEAVED_EN_BIT);
    fence();
}

void disable_cxr_interleaved() {
    *cxr_config &= ~(1u << CHIMERA_CXR_CONFIG_INTERLEAVED_EN_BIT);
    fence();
}

void enable_cxr_rob() {
    *cxr_config |= (1u << CHIMERA_CXR_CONFIG_ROB_EN_BIT);
    fence();
}

void disable_cxr_rob() {
    *cxr_config &= ~(1u << CHIMERA_CXR_CONFIG_ROB_EN_BIT);
    fence();
}

void init_mem(uint32_t *buf, size_t size) {
    for (int i = 0; i < size; i++) {
        buf[i] = (uint32_t)i;
    }
    fence();
}

void init_mem64(uint64_t *buf, size_t size) {
    for (int i = 0; i < size; i++) {
        buf[i] = (0xCAFE1234ULL << 32) | (uint64_t)i;
    }
    fence();
}

void fill32(volatile uint32_t *p, uint32_t value, size_t n) {
    for (size_t i = 0; i < n; i++) {
        p[i] = value;
    }
    fence();
}

void fill64(volatile uint64_t *p, uint64_t value, size_t n) {
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

int test_transfer_cxr_read(uint32_t *dst, int cxr_index, size_t size) {
    volatile uint32_t *src = (volatile uint32_t *)get_cxr_base(cxr_index);

    printf("Initializing CxR %d source buffer (0x%08x)...\n", cxr_index, (uint32_t)(uintptr_t)src);
    init_mem((uint32_t *)src, size);
    fence();

    printf("Clearing destination buffer (0x%08x)...\n", (uint32_t)(uintptr_t)dst);
    fill32(dst, 0xFF, size);
    fence();

    // max_llen = 0 -> page_size = 1 << OffsetWidth = one beat, so every issued burst is capped
    // at a single beat.
    uint32_t max_llen = 0;
    // uint64_t conf = DMA_CONF_DECOUPLE_NONE
    //               | (1u << IDMA_REG64_2D_CONF_SRC_REDUCE_LEN_BIT)
    //               | (max_llen << IDMA_REG64_2D_CONF_SRC_MAX_LLEN_OFFSET);
    uint64_t conf = DMA_CONF_DECOUPLE_NONE;

    printf("Starting DMA transfer of %zu bytes from CxR %d (0x%08x) to 0x%08x "
           "(reduce_len=1, max_llen=%u)...\n",
           size * sizeof(uint32_t), cxr_index, (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst,
           max_llen);
    sys_dma_blk_memcpy(
        (uintptr_t)dst,
        (uintptr_t)src,
        size * sizeof(uint32_t),
        conf
    );

    printf("DMA transfer completed. Verifying destination buffer...\n");

    int errors = 0;
    for (int i = 0; i < size; i++) {
        if (dst[i] != (uint32_t)i) {
            printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr_index, i, dst[i], (uint32_t)i);
            errors++;
        }
    }
    return errors;
}

static inline void sys_dma_wait(uint64_t tf_id) {
    while (*(sys_dma_done_ptr()) != tf_id) {
        asm volatile("nop");
    }
    fence();
}

// start N parallel transactions from N buffer sources to N CxRs
// each CxR is distanced CXR_OFFSET bytes from the previous one
int test_multi_cxr_write(uint32_t *src, size_t size, uint32_t n_cxr) {
    // Initialize source buffers
    for (uint32_t cxr = 0; cxr < n_cxr; cxr++) {
        init_mem(src + cxr * size, size);
    }

    // Clear destination buffers
    enable_cxr_multicast(n_cxr);
    fill32(CXR_BASE, 0xFF, size * n_cxr);
    disable_cxr_multicast();

    // Start DMA transfers
    volatile uint64_t dma_id; 
    for (uint32_t cxr = 0; cxr < n_cxr; cxr++) {
        dma_id = sys_dma_memcpy(
            (uintptr_t)get_cxr_base(cxr),
            (uintptr_t)(src + cxr * size),
            size * sizeof(uint32_t),
            DMA_CONF_DECOUPLE_NONE
        );
    }
    sys_dma_wait(dma_id); // Wait for the last transfer to complete

    // for (uint32_t cxr = 0; cxr < n_cxr; cxr++) {
    //     sys_dma_blk_memcpy(
    //         (uintptr_t)get_cxr_base(cxr),
    //         (uintptr_t)(src + cxr * size),
    //         size * sizeof(uint32_t),
    //         DMA_CONF_DECOUPLE_NONE
    //     );
    // }
    // fence();

    // Verify destination buffers
    int errors = 0;
    for (uint32_t cxr = 0; cxr < n_cxr; cxr++) {
        volatile uint32_t *curr_cxr = (volatile uint32_t *)get_cxr_base(cxr);
        for (uint32_t i = 0; i < size; i++) {
            if (curr_cxr[i] != (uint32_t)i) {
                printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr, i, curr_cxr[i], (uint32_t)i);
                errors++;
            }
        }
    }

    return errors;
}

int test_multi_cxr_read(uint32_t *dst, size_t size, uint32_t n_cxr) {
    printf("Transfer size: %zu bytes, Number of CxRs: %u\n", n_cxr * size * sizeof(uint32_t), n_cxr);

    // Initialize source buffers
    enable_cxr_multicast(n_cxr);
    init_mem((uint32_t *)get_cxr_base(0), size);
    disable_cxr_multicast();

    // Clear destination buffers
    fill32(dst, 0xFF, size * n_cxr);

    // Start DMA transfers
    volatile uint64_t dma_id; 
    for (uint32_t cxr = 0; cxr < n_cxr; cxr++) {
        dma_id = sys_dma_memcpy(
            (uintptr_t)(dst + cxr * size),
            (uintptr_t)get_cxr_base(cxr),
            size * sizeof(uint32_t),
            DMA_CONF_DECOUPLE_NONE
        );
    }
    sys_dma_wait(dma_id);

    // Verify destination buffers
    int errors = 0;
    for (uint32_t cxr = 0; cxr < n_cxr; cxr++) {
        for (uint32_t i = 0; i < size; i++) {
            if (dst[cxr * size + i] != (uint32_t)i) {
                printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr, i, dst[cxr * size + i], (uint32_t)i);
                errors++;
            }
        }
    }

    return errors;
}

int test_multi_cxr_read_strided(uint32_t *dst, size_t size, uint32_t n_cxr) {
    if (n_cxr == 0 || size == 0) {
        return 0;
    }

    size_t total = size * n_cxr;

    fill32(dst, 0xFFFFFFFF, total);

    size_t pairs = total / 2;

    // Clear enough local 64-bit words in each CxR.
    size_t rows = (pairs + n_cxr - 1) / n_cxr;

    {
        volatile uint64_t *cxr_mem = get_cxr_base(0);
        enable_cxr_multicast(n_cxr);
        for (size_t i = 0; i < rows; i++) {
            cxr_mem[i] = 0xDEADBEEFDEADBEEFULL;
        }
        disable_cxr_multicast();
    }

    for (size_t p = 0; p < pairs; p++) {
        uint32_t cxr = p % n_cxr;
        size_t   idx = p / n_cxr;
        uint64_t pair_val = ((uint64_t)(uint32_t)(2 * p + 1) << 32) | (uint32_t)(2 * p);

        volatile uint64_t *cxr_mem = get_cxr_base(cxr);
        cxr_mem[idx] = pair_val;
    }

    printf("Transfer size: %zu bytes, Number of CxRs: %u\n", n_cxr * size * sizeof(uint32_t), n_cxr);

    fence();

    volatile uint64_t dma_id = 0;
    size_t full_rows = pairs / n_cxr;
    uint32_t remainder = (uint32_t)(pairs % n_cxr);

    *cxr_mcast_mask_lo = (uint32_t)(1); // enable ROB for higher throughput
    *cxr_mcast_mask_hi = (uint32_t)(0);

    for (size_t row = 0; row < full_rows; row++) {
        dma_id = sys_dma_2d_memcpy(
            (uintptr_t)&dst[row * n_cxr * 2],
            (uintptr_t)get_cxr_base(0) + row * sizeof(uint64_t),
            sizeof(uint64_t),
            sizeof(uint64_t),
            CXR_OFFSET,
            n_cxr,
            DMA_CONF_DECOUPLE_ALL
        );
    }

    // Partial last row, if pairs isn't a multiple of n_cxr.
    if (remainder != 0) {
        dma_id = sys_dma_2d_memcpy(
            (uintptr_t)&dst[full_rows * n_cxr * 2],
            (uintptr_t)get_cxr_base(0) + full_rows * sizeof(uint64_t),
            sizeof(uint64_t),
            sizeof(uint64_t),
            CXR_OFFSET,
            remainder,
            DMA_CONF_DECOUPLE_ALL
        );
    }

    sys_dma_wait(dma_id);

    *cxr_mcast_mask_lo = (uint32_t)(0);

    fence();

    // Verify total-size interleaved readback.
    int errors = 0;
    for (size_t k = 0; k < total; k++) {
        uint32_t got = dst[k];
        uint32_t exp = (uint32_t)k;

        if (got != exp) {
            size_t   p   = k / 2;
            uint32_t cxr = p % n_cxr;
            size_t   idx = p / n_cxr;

            printf("Mismatch at global index %zu -> CxR %u, local pair %zu: "
                   "got 0x%08x, expected 0x%08x\n",
                   k, cxr, idx, got, exp);
            errors++;
        }
    }

    return errors;
}

int test_parallel_dma_read(uint32_t *dst, uint32_t cxr_a, uint32_t cxr_b, size_t size) {
    // read the same amount of data from two different CxRs in parallel

    // Initialize source buffers
    enable_cxr_multicast(NUM_CXR);
    init_mem((uint32_t *)CXR_BASE, size);
    disable_cxr_multicast();

    // Clear destination buffers
    fill32(dst, 0xFF, size * 2);

    // Start DMA transfers
    // volatile uint64_t dma_id_a = 
    uintptr_t dst_a = (uintptr_t)(dst);
    uintptr_t dst_b = (uintptr_t)(dst + size);
    size_t dma_size = size * sizeof(uint32_t);

    *cxr_mcast_mask_lo = (uint32_t)(1);
    *cxr_mcast_mask_hi = (uint32_t)(0);

    sys_dma_memcpy(
        (uintptr_t)(dst_a),
        (uintptr_t)get_cxr_base(cxr_a),
        dma_size,
        DMA_CONF_DECOUPLE_NONE
    );

    volatile uint64_t dma_id_b = sys_dma_memcpy(
        (uintptr_t)(dst_b),
        (uintptr_t)get_cxr_base(cxr_b),
        dma_size,
        DMA_CONF_DECOUPLE_NONE
    );

    // sys_dma_wait(dma_id_a);
    sys_dma_wait(dma_id_b);

    *cxr_mcast_mask_lo = (uint32_t)(0);

    // Verify destination buffers
    int errors = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (dst[i] != (uint32_t)i) {
            printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr_a, i, dst[i], (uint32_t)i);
            errors++;
        }
        if (dst[i + size] != (uint32_t)i) {
            printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr_b, i, dst[i + size], (uint32_t)i);
            errors++;
        }
    }

    return errors;
}

int test_parallel_dma_read_all_cxr(uint32_t *dst, size_t size) {
    // Initialize source buffers
    enable_cxr_multicast(NUM_CXR);
    init_mem((uint32_t *)CXR_BASE, size);
    disable_cxr_multicast();

    // Clear destination buffer
    fill32(dst, 0xFF, size * NUM_CXR);

    size_t dma_size = size * sizeof(uint32_t);

    *cxr_mcast_mask_lo = (uint32_t)(1);
    *cxr_mcast_mask_hi = (uint32_t)(0);

    // Start one DMA transfer per CxR, in order, without waiting in between.
    volatile uint64_t dma_id = 0;
    for (uint32_t cxr = 0; cxr < NUM_CXR; cxr++) {
        dma_id = sys_dma_memcpy(
            (uintptr_t)(dst + cxr * size),
            (uintptr_t)get_cxr_base(cxr),
            dma_size,
            DMA_CONF_DECOUPLE_NONE
        );
    }

    sys_dma_wait(dma_id); // Wait for the last transfer to complete

    *cxr_mcast_mask_lo = (uint32_t)(0);

    // Verify destination buffers
    int errors = 0;
    for (uint32_t cxr = 0; cxr < NUM_CXR; cxr++) {
        for (uint32_t i = 0; i < size; i++) {
            if (dst[cxr * size + i] != (uint32_t)i) {
                printf("Mismatch at CxR %d, index %d: got 0x%08x, expected 0x%08x\n", cxr, i, dst[cxr * size + i], (uint32_t)i);
                errors++;
            }
        }
    }

    return errors;
}


int test_transfer_interleaved_1D(uint32_t *dst, uint32_t *src, size_t size) {
    volatile uint64_t *dst64 = (volatile uint64_t *)dst;
    volatile uint64_t *src64 = (volatile uint64_t *)src;

    enable_cxr_interleaved();
    
    printf("Initializing source buffer (0x%08x)...\n", (uint32_t)(uintptr_t)src);
    init_mem64(src64, size / 2);
    fence();
    printf("Clearing destination buffer (0x%08x)...\n", (uint32_t)(uintptr_t)dst);
    fill64(dst64, 0xFFFFFFFFFFFFFFFFULL, size / 2);
    fence();

    printf("Starting MCU copy of %zu bytes from 0x%08x to 0x%08x...\n", size * sizeof(uint32_t), (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst);
    for (size_t i = 0; i < size / 2; i++) {
        dst64[i] = src64[i];
    }

    // Uncomment this to test assertion
    // for (size_t i = 0; i < size; i++) {
    //     dst[i] = src[i];
    // }

    disable_cxr_interleaved();

    printf("DMA transfer completed. Verifying destination buffer...\n");

    int errors = 0;
    for (int i = 0; i < size / 2; i++) {
        if (dst64[i] != ((0xCAFE1234ULL << 32) | (uint64_t)i)) {
            printf("Mismatch at index %d: got 0x%016llx, expected 0x%016llx\n", i, dst64[i], (0xCAFE1234ULL << 32) | (uint64_t)i);
            errors++;
        }
    }

    return errors;
}

int test_transfer_interleaved_2D(uint32_t *dst, uint32_t *src, size_t size) {
    volatile uint64_t *dst64 = (volatile uint64_t *)dst;
    volatile uint64_t *src64 = (volatile uint64_t *)src;

    enable_cxr_interleaved();
    
    printf("Initializing source buffer (0x%08x)...\n", (uint32_t)(uintptr_t)src);
    init_mem64(src64, size / 2);
    fence();
    printf("Clearing destination buffer (0x%08x)...\n", (uint32_t)(uintptr_t)dst);
    fill64(dst64, 0xFFFFFFFFFFFFFFFFULL, size / 2);
    fence();

    printf("Starting 2D DMA transfer of %zu bytes from 0x%08x to 0x%08x...\n", size * sizeof(uint32_t), (uint32_t)(uintptr_t)src, (uint32_t)(uintptr_t)dst);
    size_t num_elems  = size / 2;

    enable_cxr_rob();

    // ----- VERSION WITH REPREAT-2D DMA -----
    // size_t num_sweeps = num_elems / NUM_CXR;
    //
    // uintptr_t dst_addrs[num_sweeps];
    // uintptr_t src_addrs[num_sweeps];
    // for (size_t s = 0; s < num_sweeps; s++) {
    //     dst_addrs[s] = (uintptr_t)(dst64 + s * NUM_CXR);
    //     src_addrs[s] = (uintptr_t)(src64 + s * NUM_CXR);
    // }
    //
    // volatile uint64_t dma_id = 0;
    // for (size_t s = 0; s < num_sweeps; s++) {
    //     dma_id = sys_dma_2d_memcpy(
    //         dst_addrs[s],
    //         src_addrs[s],
    //         sizeof(uint64_t),
    //         sizeof(uint64_t),
    //         sizeof(uint64_t),
    //         NUM_CXR,
    //         DMA_CONF_DECOUPLE_NONE
    //     );
    // }
    // sys_dma_wait(dma_id); // Wait for the last transfer to complete
    // ---------------------------------------

    // ----- VERSION WITH REDUCED 1D DMA -----
    uint32_t max_llen = 0;
    uint64_t conf = DMA_CONF_DECOUPLE_ALL
                  | (1u << IDMA_REG64_2D_CONF_SRC_REDUCE_LEN_BIT)
                  | ((uint64_t)max_llen << IDMA_REG64_2D_CONF_SRC_MAX_LLEN_OFFSET);
    cxr_perf_arm();
    sys_dma_blk_memcpy(
        (uintptr_t)dst64,
        (uintptr_t)src64,
        num_elems * sizeof(uint64_t),
        conf
    );
    fence();
    cxr_perf_stop();
    uint32_t perf_dur_ar = *cxr_perf_dur_ar;
    uint32_t perf_dur_r  = *cxr_perf_dur_r;
    uint32_t perf_beats  = *cxr_perf_beats;
    // ---------------------------------------

    // for (int i = 0; i < 1024; i++) {
    //     asm volatile("nop");
    // }

    disable_cxr_rob();
    disable_cxr_interleaved();

    printf("2D DMA transfer completed. Verifying destination buffer...\n");

    int errors = 0;
    for (int i = 0; i < size / 2; i++) {
        if (dst64[i] != ((0xCAFE1234ULL << 32) | (uint64_t)i)) {
            printf("Mismatch at index %d: got 0x%016llx, expected 0x%016llx\n", i, dst64[i], (0xCAFE1234ULL << 32) | (uint64_t)i);
            errors++;
        }
    }

    uint32_t transferred_bytes = num_elems * sizeof(uint64_t);

    printf("HW perf monitor: dur_ar=%u cycles, dur_r=%u cycles, beats=%u (fill latency=%u cycles)\n",
           perf_dur_ar, perf_dur_r, perf_beats, perf_dur_ar - perf_dur_r);
    if (perf_dur_r != 0) {
        uint64_t hw_tput_int = transferred_bytes / perf_dur_r;
        uint64_t hw_tput_dec = (transferred_bytes % perf_dur_r) * 1000 / perf_dur_r;
        printf("HW perf monitor: steady-state throughput %llu.%03llu bytes/cycle (%u bytes over dur_r)\n",
               hw_tput_int, hw_tput_dec, (uint32_t)transferred_bytes);
    }

    return errors;
}


int main(void) {
    printf("--- CxR iDMA/multicast test ---\n");

    // write a 64 bit word in CxR 0
    // *(get_cxr_base(0)) = 0xDEADBEEFDEADBEEFULL;
    // *(get_cxr_base(1)) = 0x0123456789ABCDEFULL;

    // same but with the 2D DMA (two 64 bit words, each in a different CxR)
    // input buffer
    // uint64_t* WIDE_L2_BUFFER = (uint64_t*)L2_BUFFER;
    // WIDE_L2_BUFFER[0] = 0xDEADBEEFACDCACDCULL;
    // WIDE_L2_BUFFER[1] = 0x0123456789ABCDEFULL;

    // *cxr_mcast_mask_lo = (uint32_t)(1);
    // *cxr_mcast_mask_hi = (uint32_t)(0);

    // sys_dma_2d_blk_memcpy(
    //     (uintptr_t)get_cxr_base(0),
    //     (uintptr_t)WIDE_L2_BUFFER,
    //     sizeof(uint64_t),
    //     CXR_OFFSET,
    //     sizeof(uint64_t),
    //     2,                           // two reps (two CxRs)
    //     DMA_CONF_DECOUPLE_NONE
    // );

    // printf("Test 1: unicast iDMA write to CxR[0]\n");
    // int errors = test_transfer_unicast(get_cxr_base(0), L2_BUFFER, TRANSFER_SIZE);
    // printf("[%s] Test 1 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 2: unicast iDMA read from CxR[0]\n");
    // errors = test_transfer_unicast(L2_BUFFER, get_cxr_base(0), TRANSFER_SIZE);
    // printf("[%s] Test 2 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Enabling CxR multicast...\n");
    // enable_cxr_multicast(NUM_CXR);

    // printf("Test 3: multicast iDMA write to CxR[0-7]\n");
    // errors = test_transfer_mcast(get_cxr_base(0), L2_BUFFER, TRANSFER_SIZE);
    // printf("[%s] Test 3 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 4: multicast iDMA read from CxR[0-7]\n");
    // errors = test_transfer_mcast(L2_BUFFER, get_cxr_base(0), TRANSFER_SIZE);
    // printf("[%s] Test 4 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // disable_cxr_multicast();

    // printf("Test 5: multi-CxR iDMA write to CxR[0-7]\n");
    // int errors = test_multi_cxr_write(L2_BUFFER, TRANSFER_SIZE, NUM_CXR);
    // printf("[%s] Test 5 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors); 

    // printf("Test 6: multi-CxR iDMA read from CxR[0-7]\n");
    // int errors = test_multi_cxr_read(L2_BUFFER, TRANSFER_SIZE, NUM_CXR);
    // printf("[%s] Test 6 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 7: multi-CxR iDMA read from CxR[0-7] with strided access\n");
    // int errors = test_multi_cxr_read_strided(L2_BUFFER, TRANSFER_SIZE, NUM_CXR);
    // printf("[%s] Test 7 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 8: parallel iDMA read from CxR[0] and CxR[2]\n");
    // int errors = test_parallel_dma_read(L2_BUFFER, 0, 1, TRANSFER_SIZE);
    // printf("[%s] Test 8 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 9: parallel iDMA read from CxR[0-7]\n");
    // int errors = test_parallel_dma_read_all_cxr(L2_BUFFER, TRANSFER_SIZE);
    // printf("[%s] Test 9 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 10: unicast iDMA read from CxR[0] with reduce_len=8\n");
    // int errors = test_transfer_cxr_read(L2_BUFFER, 0, TRANSFER_SIZE);
    // printf("[%s] Test 10 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    // printf("Test 11: unicast iDMA read from CxR[0] with interleaved access\n");
    // int errors = test_transfer_interleaved_1D(L2_BUFFER, get_cxr_base(0), TRANSFER_SIZE);
    // printf("[%s] Test 11 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);

    printf("Test 12: unicast iDMA read from CxR[0] with interleaved access (2D)\n");
    int errors = test_transfer_interleaved_2D(L2_BUFFER, get_cxr_base(0), TRANSFER_SIZE);
    printf("[%s] Test 12 finished with %d errors\n", errors == 0 ? "PASS" : "FAIL", errors);


    return errors != 0;
}
