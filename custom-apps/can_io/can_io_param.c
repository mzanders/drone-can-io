#include "param.h"
#include "param_store.h"
#include "can_io_param.h"

/* -------------------------------------------------------------------------
 * Descriptor table — const, lives in flash
 * Defaults defined once here; param_defaults_cb() uses them automatically.
 * ------------------------------------------------------------------------- */

static const param_desc_t k_descs[] = {
    PARAM_DESC_INT  ("rc_input_backend", can_io_params_t, rc_input_backend, 0, 2,  0),
};

#define PARAM_COUNT  (sizeof(k_descs) / sizeof(k_descs[0]))

void app_param_init(void)
{
    param_init(k_descs, PARAM_COUNT);
    param_store_init(sizeof(can_io_params_t), param_defaults_cb);
}
