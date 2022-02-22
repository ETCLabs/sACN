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

#include "sacn/private/mem/merge_receiver/non_dmx.h"

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

#if SACN_MERGE_RECEIVER_ENABLED

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static MergeReceiverNonDmxNotification* sacn_pool_non_dmx;
#else
static MergeReceiverNonDmxNotification sacn_pool_non_dmx[SACN_RECEIVER_MAX_THREADS];
#endif

/*************************** Function definitions ****************************/

/*
 * Get the MergeReceiverNonDmxNotification instance for a given thread. The instance will be initialized
 * to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
MergeReceiverNonDmxNotification* get_non_dmx(sacn_thread_id_t thread_id)
{
  if (thread_id < sacn_mem_get_num_threads())
  {
    MergeReceiverNonDmxNotification* to_return = &sacn_pool_non_dmx[thread_id];
    memset(to_return, 0, sizeof(MergeReceiverNonDmxNotification));
    to_return->receiver_handle = SACN_MERGE_RECEIVER_INVALID;
    return to_return;
  }
  return NULL;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_non_dmx_buf(unsigned int num_threads)
{
  sacn_pool_non_dmx = calloc(num_threads, sizeof(MergeReceiverNonDmxNotification));
  if (!sacn_pool_non_dmx)
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

void deinit_non_dmx_buf(void)
{
  if (sacn_pool_non_dmx)
  {
    free(sacn_pool_non_dmx);
    sacn_pool_non_dmx = NULL;
  }
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_MERGE_RECEIVER_ENABLED
