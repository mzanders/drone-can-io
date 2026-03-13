/**
 * @file param.c
 * @brief Parameter descriptor layer over param_store
 */

#include "param.h"

#include <string.h>

/* -------------------------------------------------------------------------
 * Private state
 * ------------------------------------------------------------------------- */

static struct {
    const param_desc_t *descs;
    size_t              count;
} s_param;

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

static inline void *field_ptr(void *scratch, const param_desc_t *desc)
{
    return (uint8_t *)scratch + desc->offset;
}

static param_value_t clamp(const param_desc_t *desc, param_value_t val)
{
    switch (desc->type) {
    case PARAM_TYPE_INT32:
        if (val.i < (int32_t)desc->min) val.i = (int32_t)desc->min;
        if (val.i > (int32_t)desc->max) val.i = (int32_t)desc->max;
        break;
    case PARAM_TYPE_FLOAT:
        if (val.f < (float)desc->min) val.f = (float)desc->min;
        if (val.f > (float)desc->max) val.f = (float)desc->max;
        break;
    case PARAM_TYPE_BOOL:
        val.b = !!val.b;
        break;
    }
    return val;
}

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

param_err_t param_init(const param_desc_t *descs, size_t count)
{
    if (!descs || count == 0)
        return PARAM_ERR_INVALID;

    s_param.descs = descs;
    s_param.count = count;
    return PARAM_OK;
}

void param_defaults_cb(void *scratch, size_t len)
{
    (void)len;

    for (size_t i = 0; i < s_param.count; ++i) {
        const param_desc_t *d     = &s_param.descs[i];
        void               *field = field_ptr(scratch, d);

        switch (d->type) {
        case PARAM_TYPE_INT32: *(int32_t *)field = (int32_t)d->dflt; break;
        case PARAM_TYPE_FLOAT: *(float *)field   = (float)d->dflt;   break;
        case PARAM_TYPE_BOOL:  *(bool *)field    = (bool)d->dflt;    break;
        }
    }
}

/* -------------------------------------------------------------------------
 * Descriptor lookup
 * ------------------------------------------------------------------------- */

size_t param_count(void)
{
    return s_param.count;
}

const param_desc_t *param_desc_by_index(size_t index)
{
    if (index >= s_param.count)
        return NULL;
    return &s_param.descs[index];
}

const param_desc_t *param_desc_by_name(const char *name)
{
    if (!name)
        return NULL;
    for (size_t i = 0; i < s_param.count; ++i) {
        if (strcmp(s_param.descs[i].name, name) == 0)
            return &s_param.descs[i];
    }
    return NULL;
}

/* -------------------------------------------------------------------------
 * Locked access — operates on caller-held scratch pointer, no mutex ops
 * ------------------------------------------------------------------------- */

param_err_t param_get_locked(void *scratch, const param_desc_t *desc,
                              param_value_t *val_out)
{
    if (!scratch || !desc || !val_out)
        return PARAM_ERR_INVALID;

    void *field = field_ptr(scratch, desc);

    switch (desc->type) {
    case PARAM_TYPE_INT32: val_out->i = *(int32_t *)field; break;
    case PARAM_TYPE_FLOAT: val_out->f = *(float *)field;   break;
    case PARAM_TYPE_BOOL:  val_out->b = *(bool *)field;    break;
    }

    return PARAM_OK;
}

param_err_t param_set_locked(void *scratch, const param_desc_t *desc,
                              param_value_t val, param_value_t *val_out)
{
    if (!scratch || !desc)
        return PARAM_ERR_INVALID;

    val = clamp(desc, val);

    void *field = field_ptr(scratch, desc);

    switch (desc->type) {
    case PARAM_TYPE_INT32: *(int32_t *)field = val.i; break;
    case PARAM_TYPE_FLOAT: *(float *)field   = val.f; break;
    case PARAM_TYPE_BOOL:  *(bool *)field    = val.b; break;
    }

    if (val_out)
        *val_out = val;

    return PARAM_OK;
}

/* -------------------------------------------------------------------------
 * Unlocked access — thin wrappers that acquire/release the store mutex
 * ------------------------------------------------------------------------- */

param_err_t param_get(const param_desc_t *desc, param_value_t *val_out)
{
    void *scratch;
    if (param_store_lock(&scratch) != PARAM_STORE_OK)
        return PARAM_ERR_STORE;

    param_err_t err = param_get_locked(scratch, desc, val_out);

    param_store_unlock();
    return err;
}

param_err_t param_set(const param_desc_t *desc, param_value_t val,
                      param_value_t *val_out)
{
    void *scratch;
    if (param_store_lock(&scratch) != PARAM_STORE_OK)
        return PARAM_ERR_STORE;

    param_err_t err = param_set_locked(scratch, desc, val, val_out);

    param_store_unlock();
    return err;
}
