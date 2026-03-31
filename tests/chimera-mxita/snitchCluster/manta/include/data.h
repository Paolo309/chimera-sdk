// SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
// SPDX-License-Identifier: Apache-2.0
#ifndef _DATA_H
#define _DATA_H

// TODO modify data generation automation to avoid overriding this file (data.c should be overridden instead)

static uint8_t test_sink[1024];

#if   (MXITA_L ==  64) && (MXITA_K ==  8)
  #include "mxita-data/data_L64_k8.h"
#elif (MXITA_L == 64) && (MXITA_K == 32)
    #include "mxita-data/data_L64_k32.h"

#elif (MXITA_L == 128) && (MXITA_K ==  8)
  #include "mxita-data/data_L128_k8.h"
#elif (MXITA_L == 128) && (MXITA_K == 16)
  #include "mxita-data/data_L128_k16.h"
#elif (MXITA_L == 128) && (MXITA_K == 32)
  #include "mxita-data/data_L128_k32.h"
#elif (MXITA_L == 128) && (MXITA_K == 64)
  #include "mxita-data/data_L128_k64.h"

#elif (MXITA_L == 256) && (MXITA_K ==  8)
  #include "mxita-data/data_L256_k8.h"
#elif (MXITA_L == 256) && (MXITA_K == 32)
  #include "mxita-data/data_L256_k32.h"
#elif (MXITA_L == 256) && (MXITA_K == 64)
  #include "mxita-data/data_L256_k64.h"

#elif (MXITA_L == 512) && (MXITA_K == 8)
  #include "mxita-data/data_L512_k8.h"
#elif (MXITA_L == 512) && (MXITA_K == 16)
  #include "mxita-data/data_L512_k16.h"
#elif (MXITA_L == 512) && (MXITA_K == 32)
  #include "mxita-data/data_L512_k32.h"
#elif (MXITA_L == 512) && (MXITA_K == 64)
  #include "mxita-data/data_L512_k64.h"

#else
  #error "Unsupported MXITA_L/MXITA_K combination. Add a matching mxita-data/data_L<MXITA_L>_k<MXITA_K>.h header (and a branch here)."
#endif

#endif // _DATA_H
