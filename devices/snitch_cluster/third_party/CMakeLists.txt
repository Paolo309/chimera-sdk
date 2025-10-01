# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Philip Wiese <wiesep@iis.ee.ethz.ch>

set(SNITCH_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/snitch-sdk)

# -----------------------------------------------------------------------------
# Device Settings
# -----------------------------------------------------------------------------
include(${SNITCH_ROOT_DIR}/cmake/tools/SnitchUtilities.cmake)

set(SN_DEVICE "snitch_cluster")
set(SN_CFG_DIR "${SNITCH_ROOT_DIR}/devices/${SN_DEVICE}/cfg")
set(SN_HW_CFG "${SN_CFG_DIR}/default.hjson" CACHE STRING
    "Path to the hardware configuration file. Use fdiv.hjson etc. to override the default.")
set(SN_TEMPLATE_DIR "${SNITCH_ROOT_DIR}/devices/${SN_DEVICE}/templates")
set(SN_DEPS_DIR "${SNITCH_ROOT_DIR}/deps" CACHE STRING "Directory for external dependencies")
set(SN_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated" CACHE PATH "Directory for generated headers")

file(MAKE_DIRECTORY "${SN_GENERATED_DIR}")

# message(STATUS "[SNITCH-SDK] Using configuration file: ${SN_HW_CFG}")
message(STATUS "[SNITCH-SDK]  Configuration File    : ${SN_HW_CFG}")
message(STATUS "[SNITCH-SDK]  Generated Files       : ${SN_GENERATED_DIR}")
# -----------------------------------------------------------------------------
# Generate Hardware Headers
# -----------------------------------------------------------------------------
generate_all_headers()
add_dependencies(runtime_cluster_snitch generate_all_headers)

# -----------------------------------------------------------------------------
# Add Subdirectories for Runtime Components
# -----------------------------------------------------------------------------
file(GLOB_RECURSE C_SOURCES
    "${SNITCH_ROOT_DIR}/alloc.c"
    "${SNITCH_ROOT_DIR}/alloc_v2.c"
    "${SNITCH_ROOT_DIR}/cls.c"
    "${SNITCH_ROOT_DIR}/cluster_interrupts.c"
    "${SNITCH_ROOT_DIR}/dm.c"
    "${SNITCH_ROOT_DIR}/dma.c"
    "${SNITCH_ROOT_DIR}/global_interrupts.c"
    "${SNITCH_ROOT_DIR}/perf_cnt.c"
    "${SNITCH_ROOT_DIR}/riscv.c"
    # WIESEP: We use the picolibc implementation of printf
    # "${SNITCH_ROOT_DIR}/printf.c"
    # "${SNITCH_ROOT_DIR}/snrt_putchar.c"
    # WIESEP: Start files are included in this runtime library
    # "${SNITCH_ROOT_DIR}/start.S"
    # "${SNITCH_ROOT_DIR}/start.c"
    "${SNITCH_ROOT_DIR}/sync.c"
    "${SNITCH_ROOT_DIR}/team.c"
    "${SNITCH_ROOT_DIR}/omp/src/eu.c"
    "${SNITCH_ROOT_DIR}/omp/src/kmp.c"
    "${SNITCH_ROOT_DIR}/omp/src/omp.c"
)


target_sources(runtime_cluster_snitch PRIVATE ${C_SOURCES})

# Remove -Wundefined-inline warnings from snitch sources (only for these files)
set_source_files_properties(
    ${C_SOURCES}
    PROPERTIES APPEND_STRING PROPERTY COMPILE_FLAGS "-Wno-undefined-inline"
)

target_include_directories(runtime_cluster_snitch
PUBLIC
    ${SN_DEPS_DIR}
    ${SN_GENERATED_DIR}
    ${SNITCH_ROOT_DIR}/target/common/inc
    ${SNITCH_ROOT_DIR}/core/inc
    ${SNITCH_ROOT_DIR}/omp/inc
)

target_include_directories(runtime_cluster_snitch
PRIVATE
    ${SNITCH_ROOT_DIR}/core/src
    ${SNITCH_ROOT_DIR}/omp/src
)

if(SIMULATION_BACKEND STREQUAL "RTL")
    # target_include_directories(runtime_cluster_snitch PUBLIC
    #     ${SNITCH_ROOT_DIR}/target/rtl/inc
    # )

    message(ERROR "[SNITCH-SDK] Unsupported backend: ${SIMULATION_BACKEND}")
endif()

add_dependencies(runtime_cluster_snitch generate_all_headers)
