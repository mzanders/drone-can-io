/**
 * @file param.h
 * @brief Parameter descriptor layer over param_store
 *
 * Provides named, typed, range-checked access to the flat parameter blob
 * managed by param_store. Designed to map directly onto the DroneCAN
 * uavcan.protocol.param.GetSet service.
 *
 * Supported types: int32, float, bool.
 * String parameters are not supported — use a separate mechanism if needed.
 *
 * Two access styles
 * -----------------
 * Unlocked (param_get / param_set):
 *   Each call acquires and releases the param_store mutex internally.
 *   Convenient for single-field access from independent call sites.
 *
 * Locked (param_get_locked / param_set_locked):
 *   Caller holds the param_store lock via param_store_lock() / param_store_unlock().
 *   Use when batching multiple get/set operations under one lock — avoids
 *   repeated mutex round-trips and prevents interleaving between operations.
 *   Required when calling from a context that already holds the lock.
 *
 * DroneCAN mapping
 * ----------------
 *   GetSet handler (single param):  param_get() / param_set()
 *   GetSet handler (batch):         param_store_lock() → param_{get,set}_locked()
 *                                   × N → param_store_unlock()
 *   ExecuteOpcode SAVE  (0):        param_store_save()
 *   ExecuteOpcode ERASE (1):        param_store_reset()
 *
 * Boot sequence
 * -------------
 *   param_init(k_descs, PARAM_COUNT);               // register table first
 *   param_store_init(sizeof(my_params_t),            // defaults auto-generated
 *                    param_defaults_cb);             // from descriptor table
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "param_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Value type — mirrors uavcan.protocol.param.Value (int/float/bool subset)
 * ------------------------------------------------------------------------- */

typedef enum {
    PARAM_TYPE_INT32 = 0,
    PARAM_TYPE_FLOAT = 1,
    PARAM_TYPE_BOOL  = 2,
} param_type_t;

typedef union {
    int32_t i;   /**< PARAM_TYPE_INT32  */
    float   f;   /**< PARAM_TYPE_FLOAT */
    bool    b;   /**< PARAM_TYPE_BOOL  */
} param_value_t;

/* -------------------------------------------------------------------------
 * Error codes
 * ------------------------------------------------------------------------- */

typedef enum {
    PARAM_OK            =  0,
    PARAM_ERR_INVALID   = -1,  /**< NULL args or uninitialised          */
    PARAM_ERR_NOT_FOUND = -2,  /**< No parameter with that name/index   */
    PARAM_ERR_TYPE      = -3,  /**< Value type mismatch                 */
    PARAM_ERR_STORE     = -4,  /**< Underlying param_store error        */
} param_err_t;

/* -------------------------------------------------------------------------
 * Descriptor — compile-time const, lives in flash
 * ------------------------------------------------------------------------- */

/**
 * Descriptor for one parameter. Declare as static const — zero RAM cost.
 * Use the PARAM_DESC_* macros rather than initialising directly.
 */
typedef struct {
    const char  *name;
    param_type_t type;
    size_t       offset;  /**< offsetof(your_struct, field)                  */
    double       min;     /**< Numeric min (inclusive). Ignored for bool.     */
    double       max;     /**< Numeric max (inclusive). Ignored for bool.     */
    double       dflt;    /**< Factory default — used by param_defaults_cb(). */
} param_desc_t;

#define PARAM_DESC_INT(name_, st_, field_, min_, max_, dflt_) \
    { (name_), PARAM_TYPE_INT32, offsetof(st_, field_), \
      (double)(min_), (double)(max_), (double)(dflt_) }

#define PARAM_DESC_FLOAT(name_, st_, field_, min_, max_, dflt_) \
    { (name_), PARAM_TYPE_FLOAT, offsetof(st_, field_), \
      (double)(min_), (double)(max_), (double)(dflt_) }

#define PARAM_DESC_BOOL(name_, st_, field_, dflt_) \
    { (name_), PARAM_TYPE_BOOL, offsetof(st_, field_), \
      0.0, 1.0, (double)(dflt_) }

/* -------------------------------------------------------------------------
 * Initialisation
 * ------------------------------------------------------------------------- */

/**
 * @brief Register the descriptor table.
 *
 * Must be called before param_store_init() so that param_defaults_cb()
 * can populate defaults from the table on first boot.
 *
 * Does not take ownership — array must remain valid for application lifetime.
 *
 * @param descs  Pointer to descriptor array.
 * @param count  Number of entries.
 */
param_err_t param_init(const param_desc_t *descs, size_t count);

/**
 * @brief Defaults callback for param_store_init().
 *
 * Walks the descriptor table and writes each dflt value into scratch.
 * Pass this directly to param_store_init():
 *
 *   param_store_init(sizeof(my_params_t), param_defaults_cb);
 *
 * param_init() must have been called first.
 */
void param_defaults_cb(void *scratch, size_t len);

/* -------------------------------------------------------------------------
 * Descriptor lookup
 * ------------------------------------------------------------------------- */

/** @brief Total number of registered parameters. */
size_t param_count(void);

/**
 * @brief Look up descriptor by index (0-based).
 * Returns NULL if index >= param_count().
 * Iterate 0..param_count()-1 for GetSet enumeration.
 */
const param_desc_t *param_desc_by_index(size_t index);

/**
 * @brief Look up descriptor by name. Returns NULL if not found.
 */
const param_desc_t *param_desc_by_name(const char *name);

/* -------------------------------------------------------------------------
 * Unlocked access — acquires/releases store mutex on each call
 * ------------------------------------------------------------------------- */

/**
 * @brief Read a parameter value from the scratch buffer.
 */
param_err_t param_get(const param_desc_t *desc, param_value_t *val_out);

/**
 * @brief Write a parameter value into the scratch buffer.
 *
 * Numeric values are clamped to [min, max].
 * val_out (optional) receives the actual post-clamp value.
 */
param_err_t param_set(const param_desc_t *desc, param_value_t val,
                      param_value_t *val_out);

/* -------------------------------------------------------------------------
 * Locked access — caller must hold param_store lock
 * ------------------------------------------------------------------------- */

/**
 * @brief Read a parameter using an already-held scratch pointer.
 *
 * @param scratch  Pointer obtained from param_store_lock().
 */
param_err_t param_get_locked(void *scratch, const param_desc_t *desc,
                              param_value_t *val_out);

/**
 * @brief Write a parameter using an already-held scratch pointer.
 *
 * Numeric values are clamped to [min, max].
 * val_out (optional) receives the actual post-clamp value.
 *
 * @param scratch  Pointer obtained from param_store_lock().
 */
param_err_t param_set_locked(void *scratch, const param_desc_t *desc,
                              param_value_t val, param_value_t *val_out);

#ifdef __cplusplus
}
#endif
