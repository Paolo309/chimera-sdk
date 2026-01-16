// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0

#ifndef _TEST_CLUSTER_INCLUDE_GUARD_
#define _TEST_CLUSTER_INCLUDE_GUARD_

#include <stdint.h>

void clusterInterruptHandler();

int32_t mxita_test_default(void *args);
void clusterInterruptHandler_test_default();

int32_t mxita_test_b2b(void *args);
void clusterInterruptHandler_test_b2b();

int32_t mxita_test_2csc(void *args);
void clusterInterruptHandler_test_2csc();

int32_t mxita_test_3csc(void *args);
void clusterInterruptHandler_test_3csc();

int32_t mxita_test_2cores(void *args);
void clusterInterruptHandler_test_2cores();

int32_t mxita_test_rw4c(void *args);
void clusterInterruptHandler_test_rw4c();

int32_t testOtherCluster(void *args);

#endif //_TEST_CLUSTER_INCLUDE_GUARD_
