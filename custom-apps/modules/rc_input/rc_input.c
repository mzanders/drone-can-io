/****************************************************************************
 * src/modules/rc_input/rc_input.c
 ****************************************************************************/

#include "rc_input.h"
#include "sbus_backend.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include <sched.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RC_INPUT_THREAD_STACK     1536
#define RC_INPUT_THREAD_PRIORITY  110

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct rc_input_s
{
  const struct rc_input_backend_ops_s *ops;
  void                                *priv;       /* backend-allocated */

  struct rc_input_frame_s              frame;
  pthread_mutex_t                      lock;

  pthread_t                            thread;
  volatile bool                        running;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct rc_input_backend_ops_s *g_backends[] =
{
  [RC_INPUT_BACKEND_SBUS]     = &g_sbus_backend_ops,
  [RC_INPUT_BACKEND_CRSF]     = NULL,
  [RC_INPUT_BACKEND_DRONECAN] = NULL,
};

static const char * const g_backend_names[] =
{
  [RC_INPUT_BACKEND_SBUS]     = "sbus",
  [RC_INPUT_BACKEND_CRSF]     = "crsf",
  [RC_INPUT_BACKEND_DRONECAN] = "dronecan",
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void *rc_input_thread(void *arg)
{
  struct rc_input_s      *h = (struct rc_input_s *)arg;
  struct rc_input_frame_s tmp;

  memset(&tmp, 0, sizeof(tmp));

  while (h->running)
    {
      if (h->ops->read_frame(h->priv, &tmp))
        {
          pthread_mutex_lock(&h->lock);
          memcpy(&h->frame, &tmp, sizeof(tmp));
          pthread_mutex_unlock(&h->lock);
        }
    }

  h->ops->close(h->priv);  /* also frees priv */
  h->priv = NULL;
  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int rc_input_init(const struct rc_input_config_s *config,
                  struct rc_input_s **handle)
{
  struct rc_input_s *h;
  const struct rc_input_backend_ops_s *ops;
  pthread_attr_t attr;
  struct sched_param sparam;
  int ret;

  if (!config || !config->path || !handle)
    {
      return -EINVAL;
    }

  if ((unsigned)config->backend >=
      sizeof(g_backends) / sizeof(g_backends[0]) ||
      g_backends[config->backend] == NULL)
    {
      syslog(LOG_ERR, "rc_input: backend %d not available\n",
             config->backend);
      return -ENOTSUP;
    }

  ops = g_backends[config->backend];

  h = calloc(1, sizeof(*h));
  if (!h)
    {
      return -ENOMEM;
    }

  h->ops  = ops;
  h->priv = ops->alloc();
  if (!h->priv)
    {
      free(h);
      return -ENOMEM;
    }

  pthread_mutex_init(&h->lock, NULL);
  memset(&h->frame, 0, sizeof(h->frame));

  ret = ops->open(h->priv, config->path);
  if (ret < 0)
    {
      ops->close(h->priv);
      pthread_mutex_destroy(&h->lock);
      free(h);
      return ret;
    }

  h->running = true;

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, RC_INPUT_THREAD_STACK);
  sparam.sched_priority = RC_INPUT_THREAD_PRIORITY;
  pthread_attr_setschedparam(&attr, &sparam);

  ret = pthread_create(&h->thread, &attr, rc_input_thread, h);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      h->running = false;
      ops->close(h->priv);
      pthread_mutex_destroy(&h->lock);
      free(h);
      return -ret;
    }

  pthread_setname_np(h->thread, ops->name);

  *handle = h;
  return 0;
}

void rc_input_deinit(struct rc_input_s *handle)
{
  if (!handle) return;

  handle->running = false;
  pthread_join(handle->thread, NULL);  /* thread calls ops->close */
  pthread_mutex_destroy(&handle->lock);
  free(handle);
}

int rc_input_get_frame(struct rc_input_s *handle,
                       struct rc_input_frame_s *dst)
{
  int ret;

  if (!handle || !dst) return -EINVAL;

  ret = pthread_mutex_lock(&handle->lock);
  if (ret == 0)
    {
      memcpy(dst, &handle->frame, sizeof(*dst));
      pthread_mutex_unlock(&handle->lock);
    }

  return ret;
}

const char *rc_input_backend_name(rc_input_backend_t backend)
{
  if ((unsigned)backend >=
      sizeof(g_backend_names) / sizeof(g_backend_names[0]))
    {
      return "unknown";
    }
  return g_backend_names[backend];
}
