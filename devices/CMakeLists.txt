# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Philip Wiese <wiesep@iis.ee.ethz.ch>

################################################################################
# Trampoline Functions
################################################################################
if (NOT ${TARGET_PLATFORM} STREQUAL "chimera-host")
  add_library(trampoline_rv32im STATIC)
  target_compile_options(trampoline_rv32im
    PRIVATE
    -O2
    --target=riscv32-unknown-elf
    -march=rv32im
    -mabi=ilp32
  )
endif()

################################################################################
# Device Libraries
################################################################################
set(DEVICE_MAPPINGS
    chimera-convolve:snitch_cluster
    chimera-open:snitch_cluster
    chimera-host:
)

# Call the macro
add_chimera_subdirectories(${TARGET_PLATFORM} "Device" "${DEVICE_MAPPINGS}")


