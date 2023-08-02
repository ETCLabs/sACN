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

#include "sacn/private/mem/common.h"

/**************************** Private variables ******************************/

static unsigned int num_threads;

/*************************** Function definitions ****************************/

#if SACN_DYNAMIC_MEM
size_t sacn_mem_grow_capacity(size_t old_capacity, size_t capacity_requested)
{
  size_t capacity = old_capacity;
  while (capacity < capacity_requested)
    capacity *= 2;
  return capacity;
}
#endif  // SACN_DYNAMIC_MEM

unsigned int sacn_mem_get_num_threads(void)
{
  return num_threads;
}

void sacn_mem_set_num_threads(unsigned int number_of_threads)
{
  if (!SACN_ASSERT_VERIFY(number_of_threads > 0))
    return;

  num_threads = number_of_threads;
}
