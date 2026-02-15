# SPDX-FileCopyrightText: 2025 ETH Zurich and University of Bologna
# SPDX-License-Identifier: Apache-2.0

set(ABI_HOST ilp32)
set(ISA_HOST rv32imc)
set(PICOLIB_HOST rv32im/ilp32)
# WIESEP: To avoid compatibility issues, link against RV32IM libraries
set(COMPILERRT_HOST rv32im)

set(ABI_CLUSTER_SNITCH ilp32)
set(ISA_CLUSTER_SNITCH rv32imafd_zfh_xdma_xssr_xfrep)
# set(ISA_CLUSTER_SNITCH rv32imafd_zfh_xdma_xssr_xfrep_xfquarter_xfvechalf_xfvecsingle_xfexpauxvechalf_xfauxvecsingle_xfvecquarter_xfexpauxvecquarter)
# set(ISA_CLUSTER_SNITCH rv32ima_xdma)
set(PICOLIB_CLUSTER_SNITCH rv32im/ilp32)
# WIESEP: To avoid compatibility issues, link against RV32IM libraries
set(COMPILERRT_CLUSTER_SNITCH rv32im)
