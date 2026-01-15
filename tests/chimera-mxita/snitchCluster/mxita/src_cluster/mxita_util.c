#include "mxita_util.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

// MXITA HWPE cfg
void mxita_cfg(uint8_t k_size, uint16_t l_size, uint8_t lk_size, 
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

void hwpe_trigger_job() {
    HWPE_WRITE(0, MXITA_TRIGGER);
}

int hwpe_acquire_job() {
    return HWPE_READ(MXITA_ACQUIRE);
}

/**
 * @brief Reinterpret uint32_t as float (no conversion).
 *
 * @param b uint32_t value to convert.
 *
 * @returns float Converted float value.
 */
float uint32_to_float(uint32_t b) {
    float f;
    memcpy(&f, &b, sizeof(f));
    return f ;
}

/**
 * @brief Interrupt handler for the cluster, which clears the interrupt flag for the current hart.
 *
 * @warning Stack, thread and global pointer might not yet be set up!
 */
__attribute__((naked))
void clusterInterruptHandler() {
    _SET_CLUSTER_BUSY();
    _SETUP_GP();
    _CLEAR_MSIP();
}
