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

#include <Windows.h>

static void (*keyboard_interrupt_handler)();

BOOL WINAPI ConsoleSignalHandler(DWORD signal)
{
  if (signal == CTRL_C_EVENT && keyboard_interrupt_handler)
  {
    keyboard_interrupt_handler();
    return TRUE;
  }
  return FALSE;
}

void install_keyboard_interrupt_handler(void (*handler)())
{
  if (SetConsoleCtrlHandler(ConsoleSignalHandler, TRUE))
  {
    keyboard_interrupt_handler = handler;
  }
}
