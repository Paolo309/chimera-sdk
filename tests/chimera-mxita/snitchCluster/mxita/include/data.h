// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0
#ifndef _DATA_H
#define _DATA_H

// TODO modify data generation automation to avoid overriding this file (data.c should be overridden instead)

extern uint16_t l_size;
extern uint8_t k_size;
extern int8_t input_matrix[1024];
extern int8_t weight_matrix[2048];
extern uint8_t input_scale[128];
extern uint8_t weight_scale[256];
extern float output_matrix[512];
extern float result[512];

#endif // _DATA_H
