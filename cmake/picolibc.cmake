# Copyright 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Viviane Potocnik <vivianep@iis.ee.ethz.ch>

include(ExternalProject)

# message(STATUS "[CHIMERA-SDK] Setting up picolibc")

set(CROSS_C_COMPILER "${CMAKE_C_COMPILER}")
set(CROSS_C_COMPILER_ARGS "-target ${CROSS_COMPILE_HOST} -nostdlib" CACHE STRING "Compiler arguments (Host)")
# VIVIANEP: These flags are only for building Picolibc; adding them globally breaks
#           app builds (e.g., missing <sys/types.h>) or causes issues in freestanding mode.
set(CROSS_C_ARGS "-ggdb -gdwarf-4 -gstrict-dwarf -Werror=double-promotion -Wno-unsupported-floating-point-opt -fshort-enums ${CMAKE_ALT_C_OPTIONS}")
set(CROSS_C_LINK_ARGS "-Wl,-z,noexecstack")

set(CROSS_AR "${CMAKE_AR}")
set(CROSS_STRIP "${CMAKE_STRIP}")

set(CROSS_CPU "${HOST_ARCH}")
set(CROSS_CPU_FAMILY "${HOST_FAMILY}")
set(CROSS_ENDIAN "${HOST_ENDIAN}")
set(CROSS_SYSTEM "${HOST_SYSTEM}")

set(CROSS_SKIP_SANITY_CHECK "true")

# Prepare Meson arrays
function(prepare_meson_array output_var input_string)
    string(REPLACE " " ";" temp_list "${input_string}")
    set(formatted_list "")
    foreach(item IN LISTS temp_list)
        list(APPEND formatted_list "'${item}'")
    endforeach()
    string(JOIN ", " result ${formatted_list})
    set(${output_var} "${result}" PARENT_SCOPE)
endfunction()

prepare_meson_array(CROSS_C_COMPILER_ARGS_LIST "${CROSS_C_COMPILER_ARGS}")
prepare_meson_array(CROSS_C_ARGS_LIST "${CROSS_C_ARGS}")
prepare_meson_array(CROSS_C_LINK_ARGS_LIST "${CROSS_C_LINK_ARGS}")

set(PICOLIBC_SRC_DIR ${CMAKE_BINARY_DIR}/picolibc-src)

set(PICOLIBC_BUILD_DIR ${CMAKE_BINARY_DIR}/picolibc-build)
set(PICOLIBC_INSTALL_DIR ${CMAKE_BINARY_DIR}/picolibc-install)
set(PICOLIBC_CROSS_FILE ${CMAKE_BINARY_DIR}/picolibc-cross-file.txt)

# Generate the Meson cross-file
configure_file(${CMAKE_CURRENT_LIST_DIR}/../scripts/picolibc-cross-file.txt.in ${PICOLIBC_CROSS_FILE} @ONLY)

# message(STATUS "[CHIMERA-SDK] Saving cross compilation file to ${PICOLIBC_CROSS_FILE}")
# Add picolibc as an external project
ExternalProject_Add(
    picolibc
    GIT_REPOSITORY https://github.com/picolibc/picolibc.git
    GIT_TAG main
    SOURCE_DIR ${PICOLIBC_SRC_DIR}
    BINARY_DIR ${PICOLIBC_BUILD_DIR}
    INSTALL_DIR ${PICOLIBC_INSTALL_DIR}
    CONFIGURE_COMMAND meson setup ${PICOLIBC_BUILD_DIR} ${PICOLIBC_SRC_DIR} --cross-file ${PICOLIBC_CROSS_FILE} -D multilib-list=rv32im/ilp32,rv32imafd/ilp32d --prefix ${PICOLIBC_INSTALL_DIR} --default-library=static
    BUILD_COMMAND ninja -C ${PICOLIBC_BUILD_DIR}
    INSTALL_COMMAND ninja -C ${PICOLIBC_BUILD_DIR} install
    BUILD_BYPRODUCTS
        ${PICOLIBC_INSTALL_DIR}/lib/rv32im/ilp32/libc.a
        ${PICOLIBC_INSTALL_DIR}/lib/rv32imafd/ilp32d/libc.a
    LOG_CONFIGURE ON
    # LOG_BUILD ON
    LOG_INSTALL ON
)

set(PICOLIBC_TARGET picolibc)

################################################################################
# Host Picolibc Library
################################################################################
add_library(picolibc_host STATIC IMPORTED GLOBAL)

if ("${ISA_HOST}" MATCHES "rv32im")
    message(STATUS "[CHIMERA-SDK] Picolibc Host         : rv32im/ilp32")
    set_target_properties(picolibc_host PROPERTIES
        IMPORTED_LOCATION "${PICOLIBC_INSTALL_DIR}/lib/rv32im/ilp32/libc.a"
    )
else()
    message(FATAL_ERROR "[CHIMERA-SDK] Unsupported ISA_HOST for picolibc: ${ISA_HOST}.")
endif()


add_dependencies(picolibc_host picolibc)

################################################################################
# Snitch Picolibc Library
################################################################################
add_library(picolibc_cluster_snitch STATIC IMPORTED GLOBAL)

if ("${ISA_CLUSTER_SNITCH}" MATCHES "rv32im")
    if ("${ISA_CLUSTER_SNITCH}" MATCHES "fd")
        message(STATUS "[CHIMERA-SDK] Picolibc Snitch       : rv32imafd/ilp32d")
        set_target_properties(picolibc_cluster_snitch PROPERTIES
            IMPORTED_LOCATION "${PICOLIBC_INSTALL_DIR}/lib/rv32imafd/ilp32d/libc.a"
        )
    else()
        message(STATUS "[CHIMERA-SDK] Picolibc Snitch       : rv32im/ilp32")
        set_target_properties(picolibc_cluster_snitch PROPERTIES
            IMPORTED_LOCATION "${PICOLIBC_INSTALL_DIR}/lib/rv32im/ilp32/libc.a"
        )
    endif()
else()
message(FATAL_ERROR "[CHIMERA-SDK] Unsupported ISA_CLUSTER_SNITCH for picolibc: ${ISA_CLUSTER_SNITCH}.")
endif()

add_dependencies(picolibc_cluster_snitch picolibc)

