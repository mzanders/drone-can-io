/**
 * @file param_store_example.c
 * @brief param_store usage — init, DroneCAN GetSet, ExecuteOpcode
 */

#include "param_store.h"
#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * User parameter struct — application-defined
 * ------------------------------------------------------------------------- */

typedef struct {
    float    pid_p;
    float    pid_i;
    float    pid_d;
    uint16_t throttle_min;
    uint16_t throttle_max;
    uint8_t  can_node_id;
    uint8_t  mode;
    char     node_name[32];
} my_params_t;

/* -------------------------------------------------------------------------
 * Defaults callback — registered at init, called on first boot and reset
 * ------------------------------------------------------------------------- */

static void my_defaults(void *scratch, size_t len)
{
    (void)len;
    my_params_t *p = (my_params_t *)scratch;
    p->pid_p        = 0.5f;
    p->pid_i        = 0.1f;
    p->pid_d        = 0.05f;
    p->throttle_min = 1000;
    p->throttle_max = 2000;
    p->can_node_id  = 42;
    p->mode         = 0;
    strncpy(p->node_name, "my-node", sizeof(p->node_name));
}

/* -------------------------------------------------------------------------
 * Boot — single call, always succeeds or halts
 * ------------------------------------------------------------------------- */

void app_params_init(void)
{
    param_store_err_t err = param_store_init(sizeof(my_params_t), my_defaults);
    if (err != PARAM_STORE_OK) {
        /* handle fatal flash error */
    }
    /* scratch now contains valid params regardless of whether it was
     * a first boot (defaults written) or a normal boot (flash loaded) */
}

/* -------------------------------------------------------------------------
 * Application init — read params without caring about flash state
 * ------------------------------------------------------------------------- */

void app_init(void)
{
    app_params_init();

    my_params_t *p;
    param_store_lock((void **)&p);
    uint8_t node_id = p->can_node_id;
    uint8_t mode    = p->mode;
    param_store_unlock();

    /* configure CAN stack, select operating mode, etc. */
    (void)node_id;
    (void)mode;
}

/* -------------------------------------------------------------------------
 * DroneCAN: uavcan.protocol.param.GetSet handler (libcanard style)
 *
 * The descriptor table (name → offsetof + type) lives outside param_store.
 * This shows just the store interaction.
 * ------------------------------------------------------------------------- */

void dronecan_handle_getset(uint16_t param_index, const char *name,
                             bool is_set, int64_t new_int_value)
{
    my_params_t *p;
    param_store_err_t err = param_store_lock((void **)&p);
    if (err != PARAM_STORE_OK)
        return;  /* send error response */

    /* Descriptor table lookup by name or index would go here.
     * For illustration: direct field access. */
    if (is_set) {
        if (strcmp(name, "can_node_id") == 0)
            p->can_node_id = (uint8_t)new_int_value;
    }
    /* Read back value for response (still holding lock, pointer valid) */
    int64_t response_value = p->can_node_id;

    param_store_unlock();

    /* Encode and send GetSet response with response_value.
     * Note: no flash write here — changes sit in scratch until SAVE. */
    (void)response_value;
    (void)param_index;
}

/* -------------------------------------------------------------------------
 * DroneCAN: uavcan.protocol.param.ExecuteOpcode handler
 * ------------------------------------------------------------------------- */

bool dronecan_handle_execute_opcode(uint8_t opcode)
{
    param_store_err_t err;

    switch (opcode) {
    case 0:  /* OPCODE_SAVE  — flush scratch to flash */
        err = param_store_save();
        break;
    case 1:  /* OPCODE_ERASE — reset to defaults and persist */
        err = param_store_reset();
        break;
    default:
        return false;
    }

    return (err == PARAM_STORE_OK);
}
