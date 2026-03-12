/****************************************************************************
 * src/modules/rc_input/example_usage.c
 *
 * Example NuttX application using the rc_input abstraction layer.
 * Prints channel values and status every second.
 *
 * Usage: rc_example [uart_path]
 *   Default uart: /dev/ttyS1
 ****************************************************************************/

#include "rc_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RC_EXAMPLE_DEFAULT_UART  "/dev/ttyS1"
#define RC_EXAMPLE_PRINT_HZ      1

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_running = true;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void print_frame(const struct rc_input_frame_s *f)
{
  printf("frames=%-6lu errors=%-4lu rssi=",
         (unsigned long)f->frame_count,
         (unsigned long)f->error_count);

  if (f->rssi == 255)
    {
      printf("n/a");
    }
  else
    {
      printf("%u%%", (unsigned)(f->rssi * 100u / 255u));
    }

  if (f->flags & RC_INPUT_FLAG_FAILSAFE)   printf("  [FAILSAFE]");
  if (f->flags & RC_INPUT_FLAG_FRAME_LOST) printf("  [FRAME LOST]");
  printf("\n");

  for (uint8_t i = 0; i < f->channel_count && i < RC_INPUT_MAX_CHANNELS; i++)
    {
      printf("  ch%-2u  %4u  (%4u us)\n",
             i + 1,
             f->channels[i],
             rc_input_to_pwm_us(f->channels[i]));
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  const char *uart = argc > 1 ? argv[1] : RC_EXAMPLE_DEFAULT_UART;
  struct rc_input_s *rc = NULL;
  struct rc_input_frame_s frame;
  int ret;

  printf("rc_example: starting SBUS on %s\n\n", uart);

  const struct rc_input_config_s cfg =
    {
      .backend = RC_INPUT_BACKEND_SBUS,
      .path    = uart,
    };

  ret = rc_input_init(&cfg, &rc);
  if (ret < 0)
    {
      fprintf(stderr, "rc_example: init failed: %d\n", ret);
      return EXIT_FAILURE;
    }

  while (g_running)
    {
      sleep(RC_EXAMPLE_PRINT_HZ);

      ret = rc_input_get_frame(rc, &frame);
      if (ret < 0)
        {
          fprintf(stderr, "rc_example: get_frame failed: %d\n", ret);
          break;
        }

      print_frame(&frame);
      printf("\n");
    }

  rc_input_deinit(rc);
  return EXIT_SUCCESS;
}