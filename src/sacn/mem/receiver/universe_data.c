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

#include "sacn/private/mem/receiver/universe_data.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_RECEIVER_ENABLED

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static UniverseDataNotification* sacn_pool_universe_data;
#else
static UniverseDataNotification sacn_pool_universe_data[SACN_RECEIVER_MAX_THREADS];
#endif

/*************************** Function definitions ****************************/

/*
 * Get the UniverseDataNotification instance for a given thread. The instance will be initialized
 * to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
UniverseDataNotification* get_universe_data(sacn_thread_id_t thread_id)
{
  if (!SACN_ASSERT_VERIFY(thread_id != SACN_THREAD_ID_INVALID))
    return NULL;

  if (thread_id < sacn_mem_get_num_threads())
  {
    UniverseDataNotification* to_return = &sacn_pool_universe_data[thread_id];
    memset(to_return, 0, sizeof(UniverseDataNotification));
    to_return->receiver_handle = SACN_RECEIVER_INVALID;
    to_return->thread_id = SACN_THREAD_ID_INVALID;
    return to_return;
  }
  return NULL;
}

etcpal_error_t init_universe_data_buf(unsigned int num_threads)
{
  if (!SACN_ASSERT_VERIFY(num_threads > 0))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  sacn_pool_universe_data = calloc(num_threads, sizeof(UniverseDataNotification));
  if (!sacn_pool_universe_data)
    return kEtcPalErrNoMem;
#else   // SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(num_threads);
  memset(sacn_pool_universe_data, 0, sizeof(sacn_pool_universe_data));
#endif  // SACN_DYNAMIC_MEM
  return kEtcPalErrOk;
}

#if SACN_DYNAMIC_MEM
void deinit_universe_data_buf(void)
{
  if (sacn_pool_universe_data)
  {
    free(sacn_pool_universe_data);
    sacn_pool_universe_data = NULL;
  }
}
#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
