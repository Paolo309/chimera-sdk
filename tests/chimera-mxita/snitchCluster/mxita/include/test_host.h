// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Philip Wiese <wiesep@iis.ee.ethz.ch>

#ifndef _TEST_HOST_INCLUDE_GUARD_
#define _TEST_HOST_INCLUDE_GUARD_

#define TESTVAL 0x050CCE55

#include "addr_maps/soc_addr_map.h"

typedef struct {
    unsigned int core_id;
    unsigned int is_dm_core;
} mxita_return_t;

typedef struct {
    unsigned int value;
    mxita_return_t mxita_return[CLUSTER_0_NUMCORES];
} offloadArgs_t;

#endif //_TEST_HOST_INCLUDE_GUARD_
