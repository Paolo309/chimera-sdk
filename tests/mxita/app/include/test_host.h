// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Philip Wiese <wiesep@iis.ee.ethz.ch>

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
    uint32_t cycles;
} offloadArgs_t;

#endif //_TEST_HOST_INCLUDE_GUARD_
