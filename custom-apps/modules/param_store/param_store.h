/**
 * @file param_store.h
 * @brief Flash-backed parameter store with ping-pong sectors — NuttX/STM32
 *
 * Design overview
 * ---------------
 * A heap-allocated RAM scratch buffer mirrors the persisted parameters.
 * All access — read and write — goes through lock()/unlock().
 * Flash is only touched by save() and reset().
 *
 * Access model
 * ------------
 *   param_store_lock()    Acquire mutex; returns pointer to scratch.
 *                         Blocks if another caller holds the lock.
 *   param_store_unlock()  Release mutex. No flash activity.
 *   param_store_save()    Acquires mutex internally; flushes scratch → flash.
 *   param_store_reset()   Acquires mutex internally; defaults → scratch → flash.
 *
 * DroneCAN mapping
 * ----------------
 *   GetSet handler:           lock() → read/modify field → unlock()
 *   ExecuteOpcode SAVE  (0):  save()
 *   ExecuteOpcode ERASE (1):  reset()
 *
 * Boot sequence
 * -------------
 *   param_store_init() loads the newest valid flash sector into scratch, or
 *   calls the defaults callback and persists to flash on first boot.
 *   On return, scratch always contains valid data.
 *
 * Flash layout per sector
 * -----------------------
 *   [0..3]   magic    (CONFIG_PARAM_STORE_MAGIC)
 *   [4..7]   sequence (monotonic, wraps — higher mod 2^32 wins)
 *   [8..9]   data_len
 *   [10..11] crc16    (CRC16/CCITT over data bytes only)
 *   [12..]   user data blob
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Configuration — set via Kconfig, see Kconfig file
 * ------------------------------------------------------------------------- */

#ifndef CONFIG_PARAM_STORE_SECTOR_A_ADDR
#  define PARAM_STORE_SECTOR_A_ADDR  0x0800F800UL
#else
#  define PARAM_STORE_SECTOR_A_ADDR  CONFIG_PARAM_STORE_SECTOR_A_ADDR
#endif

#ifndef CONFIG_PARAM_STORE_SECTOR_B_ADDR
#  define PARAM_STORE_SECTOR_B_ADDR  0x0800FC00UL
#else
#  define PARAM_STORE_SECTOR_B_ADDR  CONFIG_PARAM_STORE_SECTOR_B_ADDR
#endif

#ifndef CONFIG_PARAM_STORE_SECTOR_SIZE
#  define PARAM_STORE_SECTOR_SIZE    1024U
#else
#  define PARAM_STORE_SECTOR_SIZE    CONFIG_PARAM_STORE_SECTOR_SIZE
#endif

#ifndef CONFIG_PARAM_STORE_MAGIC
#  define PARAM_STORE_MAGIC          0xDEADCAFEUL
#else
#  define PARAM_STORE_MAGIC          CONFIG_PARAM_STORE_MAGIC
#endif

#define PARAM_STORE_HEADER_SIZE      12U  /* sizeof(param_store_hdr_t) */
#define PARAM_STORE_DATA_SIZE_MAX    (PARAM_STORE_SECTOR_SIZE - PARAM_STORE_HEADER_SIZE)

/* -------------------------------------------------------------------------
 * Types
 * ------------------------------------------------------------------------- */

typedef enum {
    PARAM_STORE_OK             =  0,
    PARAM_STORE_ERR_INVALID    = -1,  /**< Bad args or data_len too large    */
    PARAM_STORE_ERR_CRC        = -2,  /**< Sector present but CRC mismatch   */
    PARAM_STORE_ERR_FLASH      = -3,  /**< Flash erase/write failure         */
    PARAM_STORE_ERR_NOMEM      = -4,  /**< malloc failed                     */
    PARAM_STORE_ERR_NOT_LOCKED = -5,  /**< unlock() called without lock()    */
} param_store_err_t;

/**
 * Defaults callback — populate scratch with factory default values.
 * Called by param_store_init() on first boot and by param_store_reset().
 *
 * @param scratch  Pointer to the zeroed scratch buffer.
 * @param len      Size of the scratch buffer (== data_len passed to init).
 */
typedef void (*param_store_defaults_cb_t)(void *scratch, size_t len);

/** Flash sector header — packed, lives directly in flash. */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t sequence;
    uint16_t data_len;
    uint16_t crc16;
} param_store_hdr_t;

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialise the store, allocate scratch buffer, load parameters.
 *
 * Scans both flash sectors and loads the newest valid one into scratch.
 * If neither sector is valid (first boot), calls defaults_cb to populate
 * scratch and immediately persists to flash.
 * On return, scratch always holds valid data.
 *
 * @param data_len     sizeof(your_param_struct). Must be <= PARAM_STORE_DATA_SIZE_MAX.
 * @param defaults_cb  Callback that fills scratch with factory defaults. Must not be NULL.
 */
param_store_err_t param_store_init(size_t data_len,
                                   param_store_defaults_cb_t defaults_cb);

/**
 * @brief Acquire mutex and return pointer to scratch buffer.
 *
 * Blocks if another caller holds the lock. Use for both reading and
 * modifying parameters. Must be paired with param_store_unlock().
 *
 * @param ptr_out  Receives pointer to scratch buffer.
 */
param_store_err_t param_store_lock(void **ptr_out);

/**
 * @brief Release mutex. No flash activity.
 *
 * Returns PARAM_STORE_ERR_NOT_LOCKED (-EPERM from nxmutex) if called
 * without a prior lock().
 */
param_store_err_t param_store_unlock(void);

/**
 * @brief Flush scratch buffer to flash (ping-pong write).
 *
 * Acquires and releases mutex internally. Safe to call without
 * holding the lock — will block if a lock()/unlock() is in progress.
 */
param_store_err_t param_store_save(void);

/**
 * @brief Reset to factory defaults and persist to flash.
 *
 * Acquires and releases mutex internally. Calls the defaults_cb
 * registered at init, then performs a ping-pong write.
 */
param_store_err_t param_store_reset(void);

/** @brief Which sector is active: 0 = A, 1 = B, -1 = none. */
int param_store_active_sector(void);

#ifdef __cplusplus
}
#endif
