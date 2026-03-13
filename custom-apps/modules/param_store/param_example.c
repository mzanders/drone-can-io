/**
 * @file param_example.c
 * @brief Full stack example: param_store + param layer + DroneCAN sketch
 */

#include "param.h"
#include "param_store.h"

/* -------------------------------------------------------------------------
 * User parameter struct
 * ------------------------------------------------------------------------- */

typedef struct {
    float   pid_p;
    float   pid_i;
    float   pid_d;
    int32_t throttle_min;
    int32_t throttle_max;
    int32_t can_node_id;
    bool    esc_reverse;
} my_params_t;

/* -------------------------------------------------------------------------
 * Descriptor table — const, lives in flash
 * Defaults defined once here; param_defaults_cb() uses them automatically.
 * ------------------------------------------------------------------------- */

static const param_desc_t k_descs[] = {
    PARAM_DESC_FLOAT("pid_p",        my_params_t, pid_p,        0.0, 10.0, 0.5),
    PARAM_DESC_FLOAT("pid_i",        my_params_t, pid_i,        0.0, 10.0, 0.1),
    PARAM_DESC_FLOAT("pid_d",        my_params_t, pid_d,        0.0,  1.0, 0.05),
    PARAM_DESC_INT  ("throttle_min", my_params_t, throttle_min, 800, 2200, 1000),
    PARAM_DESC_INT  ("throttle_max", my_params_t, throttle_max, 800, 2200, 2000),
    PARAM_DESC_INT  ("can_node_id",  my_params_t, can_node_id,  1,   127,  42),
    PARAM_DESC_BOOL ("esc_reverse",  my_params_t, esc_reverse,  false),
};

#define PARAM_COUNT  (sizeof(k_descs) / sizeof(k_descs[0]))

/* -------------------------------------------------------------------------
 * Boot — param_init() first so param_defaults_cb() has the table available
 * ------------------------------------------------------------------------- */

void app_params_init(void)
{
    param_init(k_descs, PARAM_COUNT);
    param_store_init(sizeof(my_params_t), param_defaults_cb);
}

/* -------------------------------------------------------------------------
 * Application init — batch read under single lock
 * ------------------------------------------------------------------------- */

void app_init(void)
{
    app_params_init();

    my_params_t *p;
    param_store_lock((void **)&p);

    param_value_t val;

    param_get_locked(p, param_desc_by_name("can_node_id"), &val);
    uint8_t node_id = (uint8_t)val.i;

    param_get_locked(p, param_desc_by_name("esc_reverse"), &val);
    bool reverse = val.b;

    param_store_unlock();

    /* configure subsystems ... */
    (void)node_id;
    (void)reverse;
}

/* -------------------------------------------------------------------------
 * DroneCAN: uavcan.protocol.param.GetSet handler
 * ------------------------------------------------------------------------- */

void dronecan_handle_getset(uint16_t index, const char *name,
                             bool is_set, param_type_t req_type,
                             param_value_t new_val,
                             param_value_t *resp_val_out)
{
    /* Name takes priority over index per DroneCAN spec */
    const param_desc_t *desc = (name && name[0] != '\0')
                                ? param_desc_by_name(name)
                                : param_desc_by_index(index);

    if (!desc)
        return;  /* respond with empty name — signals "no such param" */

    void *scratch;
    param_store_lock(&scratch);

    if (is_set && desc->type == req_type)
        param_set_locked(scratch, desc, new_val, resp_val_out);
    else
        param_get_locked(scratch, desc, resp_val_out);

    param_store_unlock();

    /* Populate GetSet response (libcanard encoding omitted):
     *   response.name          = desc->name
     *   response.value         = *resp_val_out
     *   response.default_value = desc->dflt
     *   response.min_value     = desc->min
     *   response.max_value     = desc->max
     * Changes remain in RAM scratch until ExecuteOpcode SAVE. */
}

/* -------------------------------------------------------------------------
 * DroneCAN: uavcan.protocol.param.ExecuteOpcode handler
 * ------------------------------------------------------------------------- */

bool dronecan_handle_execute_opcode(uint8_t opcode)
{
    switch (opcode) {
    case 0: return param_store_save()  == PARAM_STORE_OK;  /* SAVE  */
    case 1: return param_store_reset() == PARAM_STORE_OK;  /* ERASE */
    default: return false;
    }
}
