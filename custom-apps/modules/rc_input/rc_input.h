/****************************************************************************
 * src/modules/rc_input/rc_input.h
 *
 * Generic RC input abstraction layer.
 *
 * Backends (SBUS, CRSF, DroneCAN) implement rc_input_backend_ops_s.
 * rc_input_s owns all state: frame, lock, and backend private data.
 * All heap allocation is internal - caller holds an opaque pointer.
 *
 * Channel range follows DroneCAN convention: 0..4095 (12-bit unsigned).
 * Midpoint: 2048. Minimum: 0. Maximum: 4095.
 ****************************************************************************/

#ifndef __MODULES_RC_INPUT_RC_INPUT_H
#define __MODULES_RC_INPUT_RC_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RC_INPUT_MAX_CHANNELS   18

#define RC_INPUT_CH_MIN         0
#define RC_INPUT_CH_MAX         4095
#define RC_INPUT_CH_MID         2048

#define RC_INPUT_FLAG_FAILSAFE    (1 << 0)
#define RC_INPUT_FLAG_FRAME_LOST  (1 << 1)

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct rc_input_frame_s
{
  uint16_t  channels[RC_INPUT_MAX_CHANNELS]; /* 0..4095 */
  uint8_t   channel_count;
  uint8_t   flags;                           /* RC_INPUT_FLAG_* */
  uint8_t   rssi;                            /* 0..255, 255=unknown */
  uint32_t  frame_count;
  uint32_t  error_count;
};

typedef enum
{
  RC_INPUT_BACKEND_SBUS     = 0,
  RC_INPUT_BACKEND_CRSF     = 1,
  RC_INPUT_BACKEND_DRONECAN = 2,
} rc_input_backend_t;

struct rc_input_backend_ops_s
{
  const char *name;

  /* Allocate backend private state. Returns NULL on failure. */
  void *(*alloc)(void);

  /* Open/configure. Returns 0 on success, negative errno on failure. */
  int   (*open)(void *priv, const char *path);

  /* Close and free priv (backend is responsible for freeing its own alloc). */
  void  (*close)(void *priv);

  /* Block until frame ready or timeout.
   * Returns true on valid frame decoded into dst. */
  bool  (*read_frame)(void *priv, struct rc_input_frame_s *dst);
};

/* Minimal init config - caller only provides backend type and path */

struct rc_input_config_s
{
  rc_input_backend_t  backend;
  const char         *path;
};

/* Opaque handle - definition internal to rc_input.c */

struct rc_input_s;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

int  rc_input_init(const struct rc_input_config_s *config,
                   struct rc_input_s **handle);

void rc_input_deinit(struct rc_input_s *handle);

int  rc_input_get_frame(struct rc_input_s *handle,
                        struct rc_input_frame_s *dst);

const char *rc_input_backend_name(rc_input_backend_t backend);

static inline uint16_t rc_input_to_pwm_us(uint16_t val)
{
  return (uint16_t)(1000u + (uint32_t)val * 1000u / RC_INPUT_CH_MAX);
}

#ifdef __cplusplus
}
#endif

#endif /* __MODULES_RC_INPUT_RC_INPUT_H */
