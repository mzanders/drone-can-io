/****************************************************************************
 * boards/arm/stm32/olimexino-stm32/src/stm32_pwm.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <syslog.h>
#include <errno.h>

#include <nuttx/board.h>
#include <nuttx/mmcsd.h>
#include <nuttx/timers/pwm.h>

#include "stm32.h"
#include "olimexino-stm32.h"
#include "stm32_pwm.h"

int stm32_pwm_setup(void)
{
    struct pwm_lowerhalf_s *tim2 = stm32_pwminitialize(2);
    struct pwm_lowerhalf_s *tim3 = stm32_pwminitialize(3);

    if(tim2 == NULL || tim3 == NULL) {
        syslog(LOG_ERR, "ERROR: stm32_pwminitialize failed\n");
        return -ENODEV;
    }

    pwm_register("/dev/pwm0", tim2);
    pwm_register("/dev/pwm1", tim3);

    return 0;
}