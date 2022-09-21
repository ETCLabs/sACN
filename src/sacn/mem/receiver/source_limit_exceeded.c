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

#include "sacn/private/mem/receiver/source_limit_exceeded.h"

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
static SourceLimitExceededNotification* source_limit_exceeded;
#else
static SourceLimitExceededNotification source_limit_exceeded[SACN_RECEIVER_MAX_THREADS];
#endif

/*************************** Function definitions ****************************/

/*
 * Get the SourceLimitExceededNotification instance for a given thread. The instance will be
 * initialized to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
SourceLimitExceededNotification* get_source_limit_exceeded(sacn_thread_id_t thread_id)
{
  if (thread_id < sacn_mem_get_num_threads())
  {
    SourceLimitExceededNotification* to_return = &source_limit_exceeded[thread_id];
    memset(to_return, 0, sizeof(SourceLimitExceededNotification));
    to_return->handle = SACN_RECEIVER_INVALID;
    to_return->thread_id = SACN_THREAD_ID_INVALID;
    return to_return;
  }
  return NULL;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_source_limit_exceeded_buf(unsigned int num_threads)
{
  source_limit_exceeded = calloc(num_threads, sizeof(SourceLimitExceededNotification));
  if (!source_limit_exceeded)
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

void deinit_source_limit_exceeded_buf(void)
{
  if (source_limit_exceeded)
  {
    free(source_limit_exceeded);
    source_limit_exceeded = NULL;
  }
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
