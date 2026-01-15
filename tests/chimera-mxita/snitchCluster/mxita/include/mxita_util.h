#ifndef _MXITA_UTIL_H
#define _MXITA_UTIL_H

#include <stdint.h>

// Constant data for all MXITA tests

#define MXITA_TCDM_ALIGN 64

#define M 8
#define N 4
#define P 4
#define Q 4

#define HWPE_ADDR_BASE 0x18040000
#define MXITA_TRIGGER 0x00
#define MXITA_ACQUIRE 0x04
#define HWPE_MXIP_ADDR (HWPE_ADDR_BASE + 0x58)
#define HWPE_WRITE(value, offset) *(int *)(HWPE_ADDR_BASE + offset) = value
#define HWPE_READ(offset) *(int *)(HWPE_ADDR_BASE + offset)

// tolerance for output comparison
#define RELATIVE_TOLERANCE 1e-2

#define _CLEAR_MSIP() \
    asm volatile( \
        "csrr t0, mhartid\n" /* Load mhartid CSR into t0 */ \
        "la t1, __base_clint\n" /* Load clint base address into t1 */ \
        "slli t0, t0, 2\n" /* Calculate the interrupt target address: t1 = t1 + (t0 * 4) */ \
        "add t1, t1, t0\n" \
        "sw zero, 0(t1)\n" /* Store 0 to the interrupt target address */ \
        :            /* No outputs */ \
        :            /* No inputs */ \
        : "t0", "t1" /* Declare clobbered registers */ \
    );

// MXITA HWPE cfg
void mxita_cfg(uint8_t k_size, uint16_t l_size, uint8_t lk_size, 
               unsigned int input_ptr, unsigned int weight_ptr, 
               unsigned int output_ptr, unsigned int input_scale_ptr,
               unsigned int weight_scale_ptr, unsigned int bf16_sel);

inline void hwpe_trigger_job();
inline int hwpe_acquire_job();
inline float uint32_to_float(uint32_t b);

void clusterInterruptHandler();

#endif // _MXITA_UTIL_H
