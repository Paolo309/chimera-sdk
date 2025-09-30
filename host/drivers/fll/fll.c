// Copyright 2024 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Lorenzo Leone <lleone@iis.ee.ethz.ch>

#include "soc.h"
#include "fll.h"

#include <stdint.h>
#include <stddef.h>

/**
 * \addtogroup drivers
 * @{
 * \defgroup drivers_fll FLL Driver
 * @ingroup hal
 * @{
 * @brief FLL driver implementation for Chimera-SDK.
 *
 */

// Get the Base address of the wanted FLL
uint32_t *getFllPtr(uint8_t fllIdx) {
    uint32_t *fllPtr = 0;

    switch (fllIdx) {
    case 0:
        fllPtr = (uint32_t *)(FLL_SOC_BASE);
        break;
    case 1:
        fllPtr = (uint32_t *)(FLL_CLU_BASE);
        break;
    }
    return fllPtr;
}
//
// Set the FLL CFG REG 1 to the desired reset value: 0xC958
void initFll(volatile uint32_t *fllPtr) {
    fllPtr += FLL_CONF1_OFFSET / 4;
    *fllPtr = (0xC958 << 16) | (*fllPtr & 0x0000FFFF);
}

// Set the Multiplication and Divider values
void setFllFreq(volatile uint32_t *fllPtr, uint32_t mult, uint32_t div) {
    volatile uint32_t *cfgPtr1 = fllPtr;

    cfgPtr1 += FLL_CONF1_OFFSET / 4;

    div = div << 26;
    *cfgPtr1 = (*cfgPtr1 & 0xC3FF0000) | (mult | div);
}

/** @} */ // End of drivers_fll group
/** @} */ // End of drivers group
