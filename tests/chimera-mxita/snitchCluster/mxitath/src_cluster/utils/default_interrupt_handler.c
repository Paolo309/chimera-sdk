#include "test_cluster.h"

// Include Target Specific Headers
#include "soc.h"

// Include Driver Headers
#include "trampoline_snitchCluster.h"

#include "mxita_util.h"

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
