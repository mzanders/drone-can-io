/****************************************************************************
 * src/modules/rc_input/backends/sbus/sbus_backend.c
 *
 * SBUS backend for rc_input abstraction layer.
 *
 * Normalizes SBUS channel values (172..1811) to 0..4095.
 * Frame sync via inter-frame gap (poll timeout).
 *
 * Hardware note: SBUS is inverted UART (100kbps, 8E2).
 * STM32F1 has no software inversion - requires external NPN inverter.
 ****************************************************************************/

#include "sbus_backend.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <syslog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SBUS_HEADER           0x0F
#define SBUS_FOOTER           0x00
#define SBUS_FRAME_LEN        25
#define SBUS_NUM_CHANNELS     16

#define SBUS_CH_MIN           172
#define SBUS_CH_MAX           1811

#define SBUS_FLAG_CH17        (1 << 0)
#define SBUS_FLAG_CH18        (1 << 1)
#define SBUS_FLAG_FRAMELOST   (1 << 2)
#define SBUS_FLAG_FAILSAFE    (1 << 3)

#define SBUS_POLL_TIMEOUT_MS  4

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct sbus_priv_s
{
  int     fd;
  uint8_t buf[SBUS_FRAME_LEN];
  uint8_t pos;
  bool    synced;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint16_t sbus_normalize(uint16_t raw)
{
  if (raw <= SBUS_CH_MIN) return RC_INPUT_CH_MIN;
  if (raw >= SBUS_CH_MAX) return RC_INPUT_CH_MAX;
  return (uint16_t)((uint32_t)(raw - SBUS_CH_MIN) * RC_INPUT_CH_MAX
                    / (SBUS_CH_MAX - SBUS_CH_MIN));
}

static void *sbus_alloc(void)
{
  struct sbus_priv_s *priv = calloc(1, sizeof(*priv));
  if (priv)
    {
      priv->fd = -1;
    }
  return priv;
}

static int sbus_open(void *priv_void, const char *path)
{
  struct sbus_priv_s *priv = (struct sbus_priv_s *)priv_void;
  struct termios tio;

  priv->fd = open(path, O_RDONLY | O_NONBLOCK);
  if (priv->fd < 0)
    {
      syslog(LOG_ERR, "sbus: open %s failed: %d\n", path, errno);
      return -errno;
    }

//  if (tcgetattr(priv->fd, &tio) != 0)
//    {
//      close(priv->fd);
//      priv->fd = -1;
//      return -errno;
//    }
//
//  cfmakeraw(&tio);
//  tio.c_cflag &= ~CSIZE;
//  tio.c_cflag |= CS8 | PARENB | CSTOPB | CREAD | CLOCAL;
//  tio.c_cflag &= ~PARODD;  /* even parity */
//  tcsetattr(priv->fd, TCSANOW, &tio);
//

  return 0;
}

static void sbus_close(void *priv_void)
{
  struct sbus_priv_s *priv = (struct sbus_priv_s *)priv_void;
  if (priv->fd >= 0)
    {
      close(priv->fd);
    }
  free(priv);
}

static bool sbus_read_frame(void *priv_void, struct rc_input_frame_s *dst)
{
  struct sbus_priv_s *priv = (struct sbus_priv_s *)priv_void;
  struct pollfd pfd = { .fd = priv->fd, .events = POLLIN };
  uint8_t byte;
  int ret;

  ret = poll(&pfd, 1, SBUS_POLL_TIMEOUT_MS);

  if (ret < 0)
    {
      return (errno == EINTR) ? false : false;
    }

  if (ret == 0)
    {
      /* Inter-frame gap - reset sync */
      priv->synced = false;
      priv->pos    = 0;
      return false;
    }

  if (read(priv->fd, &byte, 1) != 1)
    {
      return false;
    }

  if (!priv->synced)
    {
      if (byte != SBUS_HEADER) return false;
      priv->synced = true;
      priv->pos    = 0;
    }

  priv->buf[priv->pos++] = byte;

  if (priv->pos < SBUS_FRAME_LEN)
    {
      return false;
    }

  /* Full frame received */

  priv->pos    = 0;
  priv->synced = false;

  if (priv->buf[0] != SBUS_HEADER || priv->buf[24] != SBUS_FOOTER)
    {
      dst->error_count++;
      return false;
    }

  /* Unpack 16 x 11-bit channels */

  const uint8_t *d = &priv->buf[1];
  for (int i = 0; i < SBUS_NUM_CHANNELS; i++)
    {
      int      bit_offset = i * 11;
      int      byte_idx   = bit_offset / 8;
      int      bit_shift  = bit_offset % 8;
      uint32_t v = (uint32_t)d[byte_idx]
                 | ((uint32_t)d[byte_idx + 1] << 8)
                 | ((uint32_t)d[byte_idx + 2] << 16);
      dst->channels[i] = sbus_normalize((uint16_t)((v >> bit_shift) & 0x7FF));
    }

  /* Digital channels 17/18 from flags byte */

  uint8_t flags = priv->buf[23];
  dst->channels[16] = (flags & SBUS_FLAG_CH17) ? RC_INPUT_CH_MAX : RC_INPUT_CH_MIN;
  dst->channels[17] = (flags & SBUS_FLAG_CH18) ? RC_INPUT_CH_MAX : RC_INPUT_CH_MIN;
  dst->channel_count = 18;

  dst->flags = 0;
  if (flags & SBUS_FLAG_FAILSAFE)  dst->flags |= RC_INPUT_FLAG_FAILSAFE;
  if (flags & SBUS_FLAG_FRAMELOST) dst->flags |= RC_INPUT_FLAG_FRAME_LOST;

  dst->rssi = 255;  /* SBUS carries no RSSI */
  dst->frame_count++;

  return true;
}

/****************************************************************************
 * Public Data
 ****************************************************************************/

const struct rc_input_backend_ops_s g_sbus_backend_ops =
{
  .name       = "sbus",
  .alloc      = sbus_alloc,
  .open       = sbus_open,
  .close      = sbus_close,
  .read_frame = sbus_read_frame,
};
