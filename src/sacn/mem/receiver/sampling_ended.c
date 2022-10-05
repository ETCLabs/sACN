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

#include "sacn/private/mem/receiver/sampling_ended.h"

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

typedef struct SamplingEndedNotificationBuf
{
  SACN_DECLARE_RECEIVER_BUF(SamplingEndedNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SamplingEndedNotificationBuf;

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static SamplingEndedNotificationBuf* sacn_pool_sampling_ended;
#else
static SamplingEndedNotificationBuf sacn_pool_sampling_ended[SACN_RECEIVER_MAX_THREADS];
#endif

/*********************** Private function prototypes *************************/

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf);

// Dynamic memory deinitialization
static void deinit_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf);
#endif  // SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

/*
 * Get a buffer of SamplingEndedNotification instances associated with a given thread. All
 * instances in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be
 * allocated.
 */
SamplingEndedNotification* get_sampling_ended_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (!SACN_ASSERT_VERIFY(thread_id != SACN_THREAD_ID_INVALID) ||
      !SACN_ASSERT_VERIFY(thread_id < sacn_mem_get_num_threads()))
  {
    return NULL;
  }

  SamplingEndedNotificationBuf* notifications = &sacn_pool_sampling_ended[thread_id];

  CHECK_CAPACITY(notifications, size, buf, SamplingEndedNotification, SACN_RECEIVER_MAX_UNIVERSES, NULL);

  memset(notifications->buf, 0, size * sizeof(SamplingEndedNotification));
  for (size_t i = 0; i < size; ++i)
  {
    notifications->buf[i].handle = SACN_RECEIVER_INVALID;
    notifications->buf[i].thread_id = SACN_THREAD_ID_INVALID;
  }

  return notifications->buf;
}

etcpal_error_t init_sampling_ended_bufs(unsigned int num_threads)
{
  if (!SACN_ASSERT_VERIFY(num_threads > 0))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  sacn_pool_sampling_ended = calloc(num_threads, sizeof(SamplingEndedNotificationBuf));
  if (!sacn_pool_sampling_ended)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sampling_ended_buf(&sacn_pool_sampling_ended[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
#else   // SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(num_threads);
  memset(sacn_pool_sampling_ended, 0, sizeof(sacn_pool_sampling_ended));
#endif  // SACN_DYNAMIC_MEM
  return kEtcPalErrOk;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf)
{
  if (!SACN_ASSERT_VERIFY(sampling_ended_buf))
    return kEtcPalErrSys;

  sampling_ended_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SamplingEndedNotification));
  if (!sampling_ended_buf->buf)
    return kEtcPalErrNoMem;

  sampling_ended_buf->buf_capacity = INITIAL_CAPACITY;
  return kEtcPalErrOk;
}

void deinit_sampling_ended_bufs(void)
{
  if (sacn_pool_sampling_ended)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_sampling_ended_buf(&sacn_pool_sampling_ended[i]);
    free(sacn_pool_sampling_ended);
    sacn_pool_sampling_ended = NULL;
  }
}

void deinit_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf)
{
  if (!SACN_ASSERT_VERIFY(sampling_ended_buf))
    return;

  if (sampling_ended_buf->buf)
    free(sampling_ended_buf->buf);
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
