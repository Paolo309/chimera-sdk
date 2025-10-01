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
    "${SNITCH_ROOT_DIR}/core/src/*.c"
    "${SNITCH_ROOT_DIR}/core/src/*.S"
    "${SNITCH_ROOT_DIR}/omp/src/*.c"
    "${SNITCH_ROOT_DIR}/omp/src/*.S"
)

target_sources(runtime_cluster_snitch PRIVATE ${C_SOURCES})

target_include_directories(runtime_cluster_snitch
PRIVATE
    ${SNITCH_ROOT_DIR}/core/inc
    ${SNITCH_ROOT_DIR}/omp/inc
    ${SNITCH_ROOT_DIR}/target/common/inc
    ${SN_DEPS_DIR}
    ${SN_DEPS_DIR}/printf
    ${SN_GENERATED_DIR}
PUBLIC
    ${SNITCH_ROOT_DIR}/core/inc
    ${SNITCH_ROOT_DIR}/omp/inc
)

if(SIMULATION_BACKEND STREQUAL "RTL")
  target_include_directories(runtime_cluster_snitch PRIVATE
    ${SNITCH_ROOT_DIR}/target/rtl/inc
  )
else()
    message(ERROR "[SNITCH-SDK] Unsupported backend: ${SIMULATION_BACKEND}")
endif()

add_dependencies(runtime_cluster_snitch generate_all_headers)
