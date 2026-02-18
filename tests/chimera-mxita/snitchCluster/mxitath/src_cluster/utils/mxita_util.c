#include "mxita_util.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

float uint32_to_float(uint32_t b) {
    float f;
    memcpy(&f, &b, sizeof(f));
    return f ;
}

/**
 * @brief Setup the Snitch cluster interrupt handler.
 *
 * @param handler Pointer to the interrupt handler function.
 */
void setup_interruptHandler(void *handler) {
    volatile void **snitchTrapHandlerAddr =
        (volatile void **)(SOC_CTRL_BASE + CHIMERA_SNITCH_INTR_HANDLER_ADDR_REG_OFFSET);

    *snitchTrapHandlerAddr = handler;
}
