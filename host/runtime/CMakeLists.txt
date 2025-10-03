# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Victor Jung <jungvi@iis.ee.ethz.ch>


target_include_directories(runtime_host
  PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/inc
)

if(SIMULATION_BACKEND STREQUAL "RTL")
  target_compile_definitions(runtime_host PUBLIC CHIMERA_SIMULATION_BACKEND_RTL)
elseif(SIMULATION_BACKEND STREQUAL "GVSoC")
  target_compile_definitions(runtime_host PUBLIC CHIMERA_SIMULATION_BACKEND_GVSOC)
endif()

target_sources(runtime_host PRIVATE
  src/putc.c
  src/alloc.c
  src/stdio.c
  src/uart.c
  src/clint.c
)
