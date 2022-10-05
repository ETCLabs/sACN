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

#include "sacn/private/mem/receiver/to_erase.h"

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

typedef struct ToEraseBuf
{
  SACN_DECLARE_RECEIVER_BUF(SacnTrackedSource*, buf, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
} ToEraseBuf;

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static ToEraseBuf* to_erase;
#else
static ToEraseBuf to_erase[SACN_RECEIVER_MAX_THREADS];
#endif

/*********************** Private function prototypes *************************/

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_to_erase_buf(ToEraseBuf* to_erase_buf);

// Dynamic memory deinitialization
static void deinit_to_erase_buf(ToEraseBuf* to_erase_buf);
#endif  // SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

/*
 * Get a buffer of pointers to SacnTrackedSources to erase. The pointers will be initialized to
 * NULL.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be allocated.
 */
SacnTrackedSource** get_to_erase_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (!SACN_ASSERT_VERIFY(thread_id != SACN_THREAD_ID_INVALID) ||
      !SACN_ASSERT_VERIFY(thread_id < sacn_mem_get_num_threads()))
  {
    return NULL;
  }

  ToEraseBuf* to_return = &to_erase[thread_id];

  CHECK_CAPACITY(to_return, size, buf, SacnTrackedSource*, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, NULL);

  memset(to_return->buf, 0, size * sizeof(SacnTrackedSource*));
  return to_return->buf;
}

etcpal_error_t init_to_erase_bufs(unsigned int num_threads)
{
  if (!SACN_ASSERT_VERIFY(num_threads > 0))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  to_erase = calloc(num_threads, sizeof(ToEraseBuf));
  if (!to_erase)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_to_erase_buf(&to_erase[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
#else   // SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(num_threads);
  memset(to_erase, 0, sizeof(to_erase));
#endif  // SACN_DYNAMIC_MEM
  return kEtcPalErrOk;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_to_erase_buf(ToEraseBuf* to_erase_buf)
{
  if (!SACN_ASSERT_VERIFY(to_erase_buf))
    return kEtcPalErrSys;

  to_erase_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SacnTrackedSource*));
  if (!to_erase_buf->buf)
    return kEtcPalErrNoMem;

  to_erase_buf->buf_capacity = INITIAL_CAPACITY;
  return kEtcPalErrOk;
}

void deinit_to_erase_bufs(void)
{
  if (to_erase)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_to_erase_buf(&to_erase[i]);
    free(to_erase);
    to_erase = NULL;
  }
}

void deinit_to_erase_buf(ToEraseBuf* to_erase_buf)
{
  if (!SACN_ASSERT_VERIFY(to_erase_buf))
    return;

  if (to_erase_buf->buf)
    free(to_erase_buf->buf);
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
