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

#include "sacn/private/mem/receiver/sources_lost.h"

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

typedef struct SourcesLostNotificationBuf
{
  SACN_DECLARE_RECEIVER_BUF(SourcesLostNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SourcesLostNotificationBuf;

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static SourcesLostNotificationBuf* sacn_pool_sources_lost;
#else
static SourcesLostNotificationBuf sacn_pool_sources_lost[SACN_RECEIVER_MAX_THREADS];
#endif

/*********************** Private function prototypes *************************/

static void zero_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size);

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf);
static etcpal_error_t init_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size);

// Dynamic memory deinitialization
static void deinit_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf);
static void deinit_sources_lost_entry(SourcesLostNotification* notification);
#endif  // SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

/*
 * Get a buffer of SourcesLostNotification instances associated with a given thread. All instances
 * in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be allocated.
 */
SourcesLostNotification* get_sources_lost_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (!SACN_ASSERT_VERIFY(thread_id != SACN_THREAD_ID_INVALID))
    return NULL;

  if (thread_id < sacn_mem_get_num_threads())
  {
    SourcesLostNotificationBuf* notifications = &sacn_pool_sources_lost[thread_id];

    // This one cannot use CHECK_CAPACITY() because of a special case in the initialization of the
    // reallocated buffer
#if SACN_DYNAMIC_MEM
    if (size > notifications->buf_capacity)
    {
      size_t new_capacity = sacn_mem_grow_capacity(notifications->buf_capacity, size);
      SourcesLostNotification* new_buf =
          (SourcesLostNotification*)realloc(notifications->buf, new_capacity * sizeof(SourcesLostNotification));
      if (new_buf && init_sources_lost_array(&new_buf[notifications->buf_capacity],
                                             new_capacity - notifications->buf_capacity) == kEtcPalErrOk)
      {
        notifications->buf = new_buf;
        notifications->buf_capacity = new_capacity;
      }
      else
      {
        return NULL;
      }
    }
#else   // SACN_DYNAMIC_MEM
    if (size > SACN_RECEIVER_MAX_UNIVERSES)
      return NULL;
#endif  // SACN_DYNAMIC_MEM

    zero_sources_lost_array(notifications->buf, size);
    return notifications->buf;
  }

  return NULL;
}

/*
 * Add a new lost source to a SourcesLostNotification.
 *
 * [out] notification SourcesLostNotification instance to which to append the lost source.
 * [in] cid CID of the source that was lost.
 * [in] name Name of the source that was lost; copied into the notification.
 * [in] terminated Whether the source was lost because its Stream_Terminated bit was set.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_lost_source(SourcesLostNotification* notification, sacn_remote_source_t handle, const EtcPalUuid* cid,
                     const char* name, bool terminated)
{
  if (!SACN_ASSERT_VERIFY(notification) || !SACN_ASSERT_VERIFY(handle != SACN_REMOTE_SOURCE_INVALID) ||
      !SACN_ASSERT_VERIFY(cid) || !SACN_ASSERT_VERIFY(name))
  {
    return false;
  }

  CHECK_ROOM_FOR_ONE_MORE(notification, lost_sources, SacnLostSource, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

  notification->lost_sources[notification->num_lost_sources].handle = handle;
  notification->lost_sources[notification->num_lost_sources].cid = *cid;
  ETCPAL_MSVC_NO_DEP_WRN strcpy(notification->lost_sources[notification->num_lost_sources].name, name);
  notification->lost_sources[notification->num_lost_sources].terminated = terminated;
  ++notification->num_lost_sources;

  return true;
}

void zero_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size)
{
  if (!SACN_ASSERT_VERIFY(sources_lost_arr))
    return;

  for (SourcesLostNotification* notification = sources_lost_arr; notification < sources_lost_arr + size; ++notification)
  {
    notification->api_callback = NULL;
    notification->internal_callback = NULL;
    notification->handle = SACN_RECEIVER_INVALID;
    notification->num_lost_sources = 0;
    notification->thread_id = SACN_THREAD_ID_INVALID;
    notification->context = NULL;
  }
}

etcpal_error_t init_sources_lost_bufs(unsigned int num_threads)
{
  if (!SACN_ASSERT_VERIFY(num_threads > 0))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  sacn_pool_sources_lost = calloc(num_threads, sizeof(SourcesLostNotificationBuf));
  if (!sacn_pool_sources_lost)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sources_lost_buf(&sacn_pool_sources_lost[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
#else   // SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(num_threads);
  memset(sacn_pool_sources_lost, 0, sizeof(sacn_pool_sources_lost));
#endif  // SACN_DYNAMIC_MEM
  return kEtcPalErrOk;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf)
{
  if (!SACN_ASSERT_VERIFY(sources_lost_buf))
    return kEtcPalErrSys;

  sources_lost_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SourcesLostNotification));
  if (!sources_lost_buf->buf)
    return kEtcPalErrNoMem;

  sources_lost_buf->buf_capacity = INITIAL_CAPACITY;
  return init_sources_lost_array(sources_lost_buf->buf, INITIAL_CAPACITY);
}

etcpal_error_t init_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size)
{
  if (!SACN_ASSERT_VERIFY(sources_lost_arr))
    return kEtcPalErrSys;

  for (SourcesLostNotification* notification = sources_lost_arr; notification < sources_lost_arr + size; ++notification)
  {
    notification->lost_sources = calloc(INITIAL_CAPACITY, sizeof(SacnLostSource));
    if (!notification->lost_sources)
      return kEtcPalErrNoMem;
    notification->lost_sources_capacity = INITIAL_CAPACITY;
  }
  return kEtcPalErrOk;
}

void deinit_sources_lost_bufs(void)
{
  if (sacn_pool_sources_lost)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_sources_lost_buf(&sacn_pool_sources_lost[i]);
    free(sacn_pool_sources_lost);
    sacn_pool_sources_lost = NULL;
  }
}

void deinit_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf)
{
  if (!SACN_ASSERT_VERIFY(sources_lost_buf))
    return;

  if (sources_lost_buf->buf)
  {
    for (size_t i = 0; i < sources_lost_buf->buf_capacity; ++i)
    {
      deinit_sources_lost_entry(&sources_lost_buf->buf[i]);
    }
    free(sources_lost_buf->buf);
  }
}

void deinit_sources_lost_entry(SourcesLostNotification* notification)
{
  if (!SACN_ASSERT_VERIFY(notification))
    return;

  CLEAR_BUF(notification, lost_sources);
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
