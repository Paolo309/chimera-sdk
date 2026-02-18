#ifndef _MXITA_UTIL_H
#define _MXITA_UTIL_H

#include <stdint.h>
#include <string.h>

// Constant data for all MXITA tests

#define MXITA_TCDM_ALIGN 64

#define M 8
#define N 4
#define P 4
#define Q 4

#define HWPE_ADDR_BASE 0x18040000
#define MXITA_TRIGGER 0x00
#define MXITA_ACQUIRE 0x04
#define MXITA_SOFT_CLEAR 0x14
#define MXITA_PERF_COUNTER 0x88
#define HWPE_MXIP_ADDR (HWPE_ADDR_BASE + 0x58)
#define HWPE_WRITE(value, offset) *(volatile uint32_t *)(HWPE_ADDR_BASE + offset) = value
#define HWPE_READ(offset) *(volatile uint32_t *)(HWPE_ADDR_BASE + offset)

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

/**
 * @brief Configure the MXITA HWPE with the given parameters.
 *
 * @param k_size K
 * @param l_size L
 * @param lk_size LK
 * @param input_ptr Pointer to input matrix
 * @param weight_ptr Pointer to weight matrix
 * @param output_ptr Pointer to output matrix
 * @param input_scale_ptr Pointer to input scale
 * @param weight_scale_ptr Pointer to weight scale
 * @param bf16_sel BF16 selection flag (if false, use FP32)
*/
static inline void mxita_cfg(uint8_t k_size, uint16_t l_size, uint8_t lk_size, 
               unsigned int input_ptr, unsigned int weight_ptr, 
               unsigned int output_ptr, unsigned int input_scale_ptr,
               unsigned int weight_scale_ptr, unsigned int bf16_sel) {
    uint32_t l_dims_reg = 0;
    uint32_t ctrl_stream_reg = 0;
    l_dims_reg = ((uint32_t)lk_size << 24) | ((uint32_t)l_size << 8) | ((uint32_t)k_size << 0);
    HWPE_WRITE(input_ptr, 0x20);
    HWPE_WRITE(weight_ptr, 0x24);
    HWPE_WRITE(output_ptr, 0x28);
    HWPE_WRITE(l_dims_reg, 0x2C);
    HWPE_WRITE(0, 0x30); // reg_ctrl_stream
    HWPE_WRITE(input_scale_ptr, 0x34);
    HWPE_WRITE(weight_scale_ptr, 0x38);
    HWPE_WRITE(bf16_sel, 0x3C);
}

/**
 * @brief Run the MXITA HWPE.
*/
static inline void hwpe_trigger_job() {
    HWPE_WRITE(0, MXITA_TRIGGER);
}

/**
 * @brief Acquire a job from the MXITA HWPE.
 *
 * @return int Status of the acquired job.
*/
static inline int hwpe_acquire_job() {
    return HWPE_READ(MXITA_ACQUIRE);
}

/**
 * @brief Wait to acquire a job from the MXITA HWPE.
*/
static inline void hwpe_wait_acquire_job() {
    volatile int status;
    do {
        status = hwpe_acquire_job();
    } while (status < 0);
}

/**
 * @brief Soft clear the MXITA HWPE, resetting its state.
*/
static inline void hwpe_soft_clear() {
    HWPE_WRITE(0, MXITA_SOFT_CLEAR);
}

/**
 * @brief Soft clear the MXITA HWPE, keeping register values.
*/
static inline void hwpe_soft_clear_keep_regs() {
    HWPE_WRITE(1, MXITA_SOFT_CLEAR);
}

/**
 * @brief Commit the MXITA HWPE configuration.
*/
static inline void hwpe_commit() {
    HWPE_WRITE(1, MXITA_TRIGGER);
}

/**
 * @brief Clear MXITA's interrupt flag for the given core index.
 *
 * @param core_idx Core index to clear the interrupt flag for.
 */
static inline void snrt_hwpe_clr_mxip(uint32_t core_idx) {
    *(volatile uint32_t *)HWPE_MXIP_ADDR = (1 << core_idx);
}

/**
 * @brief Enable the performance counter for MXITA HWPE.
 * If it was already enabled, this has no effect unless the new
 * value is zero. In that case, the counter is stopped and reset.
 *
 * @param reps Number of repetitions to set in the performance counter.
 */
static inline void hwpe_set_perfcnt(int reps) {
    HWPE_WRITE(reps, MXITA_PERF_COUNTER);
}

/** 
 * @brief Get the current value of the MXITA HWPE performance counter.
 *
 * @return int Current value of the performance counter.
 */
static inline int hwpe_get_perfcnt() {
    return HWPE_READ(MXITA_PERF_COUNTER);
}

/**
 * @brief Convert a uint32_t representation of a float to a float.
 *
 * @param b The uint32_t representation of the float.
 * @return float The converted float.
 */
float uint32_to_float(uint32_t b);

/**
 * @brief Setup the interrupt handler for the cluster cores.
 * All cores in all clusters will jump to the handler when an interrupt is triggered.
 *
 * @param handler Function pointer to the interrupt handler
 */
void setup_interruptHandler(void *handler);

#define CORE_TYPE_STR() \
    (snrt_is_dm_core() ? "DM" : "CC")

// TODO this stuff should be defined somewhere else, but isn't (?)
#define SNRT_TCDM_BANK_WIDTH 8
#define SNRT_TCDM_BANK_NUM 64
#define SNRT_TCDM_HYPERBANK_NUM 2
#define SNRT_TCDM_BANK_PER_HYPERBANK_NUM 32 // SNRT_TCDM_BANK_NUM / SNRT_TCDM_HYPERBANK_NUM
#define SNRT_TCDM_HYPERBANK_WIDTH (SNRT_TCDM_BANK_PER_HYPERBANK_NUM * SNRT_TCDM_BANK_WIDTH)
#define SNRT_TCDM_START_ADDR 0x18000000

/**
 * @brief Align to next multiple of size from a given base.
 * @details This macro aligns the address to the next alignment boundary
 *          specified by \p size and \p base. Alignment boundaries are defined
 *          by summing integer multiples of \p size to the base address.
 * @param addr Address to be aligned
 * @param size Alignment size in bytes
 * @param base Base address for the alignment boundaries
 * @return pointer to the allocated memory
 */
static inline uintptr_t snrt_align_up(uintptr_t addr, size_t size) {
    uintptr_t base = 0;
    return (((addr - base) + size - 1) / size) * size + base;
}

static inline uintptr_t snrt_align_up_base(uintptr_t addr, size_t size,
                               uintptr_t base) {
    return (((addr - base) + size - 1) / size) * size + base;
}

static inline uintptr_t snrt_align_up_hyperbank(uintptr_t addr) {
    return snrt_align_up_base(addr, SNRT_TCDM_HYPERBANK_WIDTH, SNRT_TCDM_START_ADDR);
}

#endif // _MXITA_UTIL_H
