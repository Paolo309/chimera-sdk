// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

#ifndef _TEST_HOST_INCLUDE_GUARD_
#define _TEST_HOST_INCLUDE_GUARD_

#if defined(HARDWARE_BACKEND_RTL)
#define BACKEND_NAME "RTL"
#elif defined(HARDWARE_BACKEND_ASIC)
#define BACKEND_NAME "ASIC"
#else
#define BACKEND_NAME "Unknown"
#endif

typedef struct {
    uint8_t is_rtl;
    uint32_t bf16_sel;
    uint32_t hw_cycles;
    uint32_t sw_cycles;
} offloadArgs_t;

typedef struct {
    const char *name;
    int (*fn)(void);
} test_entry_t;

#endif //_TEST_HOST_INCLUDE_GUARD_
