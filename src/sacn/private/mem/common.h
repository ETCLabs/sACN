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

#ifndef SACN_PRIVATE_MEM_COMMON_H_
#define SACN_PRIVATE_MEM_COMMON_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SACN_DYNAMIC_MEM
#define CLEAR_BUF(ptr, buf_name) \
  do                             \
  {                              \
    if ((ptr)->buf_name)         \
      free((ptr)->buf_name);     \
    (ptr)->buf_name = NULL;      \
    (ptr)->num_##buf_name = 0;   \
  } while (0)

#define CHECK_CAPACITY(container, size_requested, buffer, buffer_type, max_static, failure_return_value)        \
  do                                                                                                            \
  {                                                                                                             \
    if (size_requested > container->buffer##_capacity)                                                          \
    {                                                                                                           \
      size_t new_capacity = sacn_mem_grow_capacity(container->buffer##_capacity, size_requested);               \
      buffer_type* new_##buffer = (buffer_type*)realloc(container->buffer, new_capacity * sizeof(buffer_type)); \
      if (new_##buffer)                                                                                         \
      {                                                                                                         \
        container->buffer = new_##buffer;                                                                       \
        container->buffer##_capacity = new_capacity;                                                            \
      }                                                                                                         \
      else                                                                                                      \
      {                                                                                                         \
        return failure_return_value;                                                                            \
      }                                                                                                         \
    }                                                                                                           \
  } while (0)

#define CHECK_ROOM_FOR_ONE_MORE(container, buffer, buffer_type, max_static, failure_return_value) \
  CHECK_CAPACITY(container, container->num_##buffer + 1, buffer, buffer_type, max_static, failure_return_value)
#else  // SACN_DYNAMIC_MEM
#define CLEAR_BUF(ptr, buf_name) (ptr)->num_##buf_name = 0

#define CHECK_CAPACITY(container, size_requested, buffer, buffer_type, max_static, failure_return_value) \
  do                                                                                                     \
  {                                                                                                      \
    if (size_requested > max_static)                                                                     \
    {                                                                                                    \
      return failure_return_value;                                                                       \
    }                                                                                                    \
  } while (0)

#define CHECK_ROOM_FOR_ONE_MORE(container, buffer, buffer_type, max_static, failure_return_value) \
  CHECK_CAPACITY(container, container->num_##buffer + 1, buffer, buffer_type, max_static, failure_return_value)
#endif  // SACN_DYNAMIC_MEM

#define INITIAL_CAPACITY 8

#define REMOVE_AT_INDEX(container, buffer_type, buffer, index)          \
  do                                                                    \
  {                                                                     \
    --container->num_##buffer;                                          \
                                                                        \
    if (index < container->num_##buffer)                                \
    {                                                                   \
      memmove(&container->buffer[index], &container->buffer[index + 1], \
              (container->num_##buffer - index) * sizeof(buffer_type)); \
    }                                                                   \
  } while (0)

size_t sacn_mem_grow_capacity(size_t old_capacity, size_t capacity_requested);

unsigned int sacn_mem_get_num_threads(void);
void sacn_mem_set_num_threads(unsigned int number_of_threads);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MEM_COMMON_H_ */
