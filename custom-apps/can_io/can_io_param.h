#pragma once

#include <stdint.h>

#include "param.h"
#include "param_store.h"

/* -------------------------------------------------------------------------
 * User parameter struct
 * ------------------------------------------------------------------------- */

typedef struct {
    int32_t rc_input_backend;  /**< 0=SBUS, 1=CRSF, 2=DroneCAN */
} can_io_params_t;

