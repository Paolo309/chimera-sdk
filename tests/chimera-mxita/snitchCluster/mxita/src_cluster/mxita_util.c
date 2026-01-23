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

__attribute__((naked))
void clusterInterruptHandler() {
    _SET_CLUSTER_BUSY();
    _SETUP_GP();
    _CLEAR_MSIP();
}
