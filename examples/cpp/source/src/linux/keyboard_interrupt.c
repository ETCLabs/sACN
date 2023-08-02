/******************************************************************************
 * Copyright 2022 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of sACN. For more information, go to:
 * https://github.com/ETCLabs/sACN
 *****************************************************************************/

#include <signal.h>
#include <stddef.h>

static void (*keyboard_interrupt_handler)();

void signal_handler(int signal)
{
  if (signal == SIGINT && keyboard_interrupt_handler)
  {
    keyboard_interrupt_handler();
  }
}

void install_keyboard_interrupt_handler(void (*handler)())
{
  struct sigaction sigint_handler;
  sigint_handler.sa_handler = signal_handler;
  sigemptyset(&sigint_handler.sa_mask);
  sigint_handler.sa_flags = 0;
  if (sigaction(SIGINT, &sigint_handler, NULL) == 0)
  {
    keyboard_interrupt_handler = handler;
  }
}
