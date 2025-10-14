# WIESEP: It is important to set the ISA and ABI for the host and the cluster snitch

set(ABI_HOST ilp32)
set(ISA_HOST rv32imc)
set(PICOLIB_HOST rv32im/ilp32)
set(COMPILERRT_HOST rv32imc)

set(ABI_CLUSTER_SNITCH ilp32)
set(ISA_CLUSTER_SNITCH rv32imafd_xdma)
set(PICOLIB_CLUSTER_SNITCH rv32imafd/ilp32d)
set(COMPILERRT_CLUSTER_SNITCH rv32imafd)
