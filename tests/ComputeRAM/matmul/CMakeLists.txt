# SPDX-FileCopyrightText: 2024 ETH Zurich and University of Bologna
# SPDX-License-Identifier: Apache-2.0

set(TEST_NAME test_ComputeRAM_matmul)

######## Source files ###########################################################
file(GLOB_RECURSE TEST_HOST_SRCS "src_host/*.c")

# Optional prebuilt host object(s) compiled externally, separated by ';'.
set(TEST_EXTERNAL_HOST_OBJS "${CMAKE_SOURCE_DIR}/tests/ComputeRAM/matmul/src_host/patch_embed_matmul.o" CACHE STRING
    "Semicolon-separated list of external prebuilt .o files to link into ${TEST_NAME}_host.elf")

if(TEST_EXTERNAL_HOST_OBJS)
    foreach(ext_obj IN LISTS TEST_EXTERNAL_HOST_OBJS)
        if(NOT IS_ABSOLUTE "${ext_obj}")
            get_filename_component(ext_obj "${ext_obj}" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
        endif()
        if(NOT EXISTS "${ext_obj}")
            message(FATAL_ERROR "External host object file not found: ${ext_obj}")
        endif()
        list(APPEND TEST_HOST_SRCS "${ext_obj}")
    endforeach()
    message(STATUS "[CHIMERA] ${TEST_NAME}: linking external host object(s): ${TEST_EXTERNAL_HOST_OBJS}")
endif()

######## HOST Binary ###########################################################
add_host_binary(${TEST_NAME}
    ISA          ${ISA_HOST}
    ABI          ${ABI_HOST}
    COMPILER     ${CROSS_COMPILE_HOST}
    COMPIERT_RT  ${COMPILERRT_HOST}
    PICOLIBC     ${PICOLIB_HOST}
    TARGET_DIR   ${CHIMERA_TARGET_DIR}
    DEVICE_DIR   ${CHIMERA_TARGET_DIR}/host
    SOURCES      ${TEST_HOST_SRCS}
)
target_include_directories(${TEST_NAME}_host.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(${TEST_NAME}_host.elf PRIVATE -O2 -ffunction-sections -fdata-sections)
target_link_options(${TEST_NAME}_host.elf PRIVATE -Wl,--gc-sections)

######## Simulation ############################################################
if(TEST_MODE STREQUAL "simulation")
    add_test(NAME ${TEST_NAME}
        COMMAND ${SIMULATION_BINARY}
            +BINARY=${CMAKE_CURRENT_BINARY_DIR}/chimera_unified_${TEST_NAME}.elf
            +PRELMODE=${PRELOAD_MODE_INT}
    )
endif()
