/**
 * @file param_store.c
 * @brief Flash-backed ping-pong parameter store — NuttX/STM32 implementation
 */

#include "param_store.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <nuttx/progmem.h>
#include <nuttx/mutex.h>

/* -------------------------------------------------------------------------
 * Private state
 * ------------------------------------------------------------------------- */

static struct {
    size_t                    data_len;
    int                       active;      /**< 0 = sector A, 1 = B, -1 = none */
    uint32_t                  sequence;
    uint8_t                  *scratch;
    mutex_t                   mutex;
    param_store_defaults_cb_t defaults_cb;
} s_ps = { .active = -1 };

static const uint32_t k_sector_addr[2] = {
    PARAM_STORE_SECTOR_A_ADDR,
    PARAM_STORE_SECTOR_B_ADDR,
};

/* -------------------------------------------------------------------------
 * CRC-16/CCITT-FALSE  poly=0x1021  init=0xFFFF
 * ------------------------------------------------------------------------- */

static uint16_t crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000U) ? ((crc << 1) ^ 0x1021U) : (crc << 1);
    }
    return crc;
}

/* -------------------------------------------------------------------------
 * Flash helpers
 * ------------------------------------------------------------------------- */

static bool sector_valid(int idx, size_t expected_len, uint32_t *out_seq)
{
    const param_store_hdr_t *hdr =
        (const param_store_hdr_t *)k_sector_addr[idx];

    if (hdr->magic    != PARAM_STORE_MAGIC)      return false;
    if (hdr->data_len != (uint16_t)expected_len) return false;

    const uint8_t *payload =
        (const uint8_t *)(k_sector_addr[idx] + PARAM_STORE_HEADER_SIZE);
    if (crc16(payload, expected_len) != hdr->crc16) return false;

    if (out_seq) *out_seq = hdr->sequence;
    return true;
}

/**
 * Erase sector idx, write header + scratch, verify.
 * Caller must hold the mutex.
 */
static param_store_err_t sector_commit(int idx, uint32_t seq)
{
    uint32_t base = k_sector_addr[idx];

    /* Resolve page number and erase */
    ssize_t page = up_progmem_getpage(base);
    if (page < 0)
        return PARAM_STORE_ERR_FLASH;

    if (up_progmem_eraseblock((size_t)page) < 0)
        return PARAM_STORE_ERR_FLASH;

    /* Build and write header */
    param_store_hdr_t hdr = {
        .magic    = PARAM_STORE_MAGIC,
        .sequence = seq,
        .data_len = (uint16_t)s_ps.data_len,
        .crc16    = crc16(s_ps.scratch, s_ps.data_len),
    };

    if (up_progmem_write(base, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr))
        return PARAM_STORE_ERR_FLASH;

    /* Write data — up_progmem_write handles alignment internally */
    if (up_progmem_write(base + PARAM_STORE_HEADER_SIZE,
                         s_ps.scratch, s_ps.data_len) != (ssize_t)s_ps.data_len)
        return PARAM_STORE_ERR_FLASH;

    /* Verify */
    uint32_t read_seq;
    if (!sector_valid(idx, s_ps.data_len, &read_seq) || read_seq != seq)
        return PARAM_STORE_ERR_CRC;

    return PARAM_STORE_OK;
}

/**
 * Write scratch to the inactive sector, bump sequence, flip active.
 * Caller must hold the mutex.
 */
static param_store_err_t flush_scratch(void)
{
    int      target  = (s_ps.active == 0) ? 1 : 0;
    uint32_t new_seq = s_ps.sequence + 1U;

    param_store_err_t err = sector_commit(target, new_seq);
    if (err != PARAM_STORE_OK)
        return err;

    s_ps.active   = target;
    s_ps.sequence = new_seq;
    return PARAM_STORE_OK;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

param_store_err_t param_store_init(size_t data_len,
                                   param_store_defaults_cb_t defaults_cb)
{
    if (data_len == 0 || data_len > PARAM_STORE_DATA_SIZE_MAX || !defaults_cb)
        return PARAM_STORE_ERR_INVALID;

    s_ps.scratch = (uint8_t *)malloc(data_len);
    if (!s_ps.scratch)
        return PARAM_STORE_ERR_NOMEM;

    nxmutex_init(&s_ps.mutex);

    s_ps.data_len    = data_len;
    s_ps.active      = -1;
    s_ps.sequence    = 0;
    s_ps.defaults_cb = defaults_cb;
    memset(s_ps.scratch, 0, data_len);

    uint32_t seq[2] = {0, 0};
    bool     valid[2];

    valid[0] = sector_valid(0, data_len, &seq[0]);
    valid[1] = sector_valid(1, data_len, &seq[1]);

    if (valid[0] || valid[1]) {
        if (valid[0] && valid[1]) {
            uint32_t diff = seq[0] - seq[1];
            s_ps.active   = (diff < 0x80000000UL) ? 0 : 1;
        } else {
            s_ps.active = valid[0] ? 0 : 1;
        }
        s_ps.sequence = seq[s_ps.active];

        const uint8_t *flash_data =
            (const uint8_t *)(k_sector_addr[s_ps.active] + PARAM_STORE_HEADER_SIZE);
        memcpy(s_ps.scratch, flash_data, data_len);
    } else {
        /* First boot — write defaults to sector A */
        defaults_cb(s_ps.scratch, data_len);
        s_ps.active   = 1;   /* flush_scratch will target 0 */
        s_ps.sequence = 0;
        param_store_err_t err = flush_scratch();
        if (err != PARAM_STORE_OK) {
            free(s_ps.scratch);
            s_ps.scratch = NULL;
            return err;
        }
    }

    return PARAM_STORE_OK;
}

param_store_err_t param_store_lock(void **ptr_out)
{
    if (!ptr_out || !s_ps.scratch)
        return PARAM_STORE_ERR_INVALID;

    int ret = nxmutex_lock(&s_ps.mutex);
    if (ret < 0)
        return PARAM_STORE_ERR_INVALID;  /* interrupted or not initialised */

    *ptr_out = s_ps.scratch;
    return PARAM_STORE_OK;
}

param_store_err_t param_store_unlock(void)
{
    int ret = nxmutex_unlock(&s_ps.mutex);
    if (ret == -EPERM)
        return PARAM_STORE_ERR_NOT_LOCKED;

    return (ret == 0) ? PARAM_STORE_OK : PARAM_STORE_ERR_INVALID;
}

param_store_err_t param_store_save(void)
{
    int ret = nxmutex_lock(&s_ps.mutex);
    if (ret < 0)
        return PARAM_STORE_ERR_INVALID;

    param_store_err_t err = flush_scratch();

    nxmutex_unlock(&s_ps.mutex);
    return err;
}

param_store_err_t param_store_reset(void)
{
    int ret = nxmutex_lock(&s_ps.mutex);
    if (ret < 0)
        return PARAM_STORE_ERR_INVALID;

    s_ps.defaults_cb(s_ps.scratch, s_ps.data_len);
    param_store_err_t err = flush_scratch();

    nxmutex_unlock(&s_ps.mutex);
    return err;
}

int param_store_active_sector(void)
{
    return s_ps.active;
}
