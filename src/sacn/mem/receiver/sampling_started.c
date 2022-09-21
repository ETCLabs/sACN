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

#include "sacn/private/mem/receiver/sampling_started.h"

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

/****************************** Private types ********************************/

typedef struct SamplingStartedNotificationBuf
{
  SACN_DECLARE_RECEIVER_BUF(SamplingStartedNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SamplingStartedNotificationBuf;

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static SamplingStartedNotificationBuf* sacn_pool_sampling_started;
#else
static SamplingStartedNotificationBuf sacn_pool_sampling_started[SACN_RECEIVER_MAX_THREADS];
#endif

/*********************** Private function prototypes *************************/

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf);

// Dynamic memory deinitialization
static void deinit_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf);
#endif  // SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

/*
 * Get a buffer of SamplingStartedNotification instances associated with a given thread. All
 * instances in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be
 * allocated.
 */
SamplingStartedNotification* get_sampling_started_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (thread_id < sacn_mem_get_num_threads())
  {
    SamplingStartedNotificationBuf* notifications = &sacn_pool_sampling_started[thread_id];

    CHECK_CAPACITY(notifications, size, buf, SamplingStartedNotification, SACN_RECEIVER_MAX_UNIVERSES, NULL);

    memset(notifications->buf, 0, size * sizeof(SamplingStartedNotification));
    for (size_t i = 0; i < size; ++i)
    {
      notifications->buf[i].handle = SACN_RECEIVER_INVALID;
      notifications->buf[i].thread_id = SACN_THREAD_ID_INVALID;
    }

    return notifications->buf;
  }

  return NULL;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_sampling_started_bufs(unsigned int num_threads)
{
  sacn_pool_sampling_started = calloc(num_threads, sizeof(SamplingStartedNotificationBuf));
  if (!sacn_pool_sampling_started)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sampling_started_buf(&sacn_pool_sampling_started[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf)
{
  SACN_ASSERT(sampling_started_buf);

  sampling_started_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SamplingStartedNotification));
  if (!sampling_started_buf->buf)
    return kEtcPalErrNoMem;

  sampling_started_buf->buf_capacity = INITIAL_CAPACITY;
  return kEtcPalErrOk;
}

void deinit_sampling_started_bufs(void)
{
  if (sacn_pool_sampling_started)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_sampling_started_buf(&sacn_pool_sampling_started[i]);
    free(sacn_pool_sampling_started);
    sacn_pool_sampling_started = NULL;
  }
}

void deinit_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf)
{
  SACN_ASSERT(sampling_started_buf);

  if (sampling_started_buf->buf)
    free(sampling_started_buf->buf);
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
