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

#include "sacn/private/mem/receiver/status_lists.h"

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
static SacnSourceStatusLists* sacn_pool_status_lists;
#else
static SacnSourceStatusLists sacn_pool_status_lists[SACN_RECEIVER_MAX_THREADS];
#endif

/*********************** Private function prototypes *************************/

static void zero_status_lists(SacnSourceStatusLists* lists);

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_status_lists_entry(SacnSourceStatusLists* lists);

// Dynamic memory deinitialization
static void deinit_status_lists_entry(SacnSourceStatusLists* lists);
#endif  // SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

/*
 * Get the SacnSourceStatusLists instance for a given thread. The instance will be initialized to
 * default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
SacnSourceStatusLists* get_status_lists(sacn_thread_id_t thread_id)
{
  if (thread_id < sacn_mem_get_num_threads())
  {
    SacnSourceStatusLists* to_return = &sacn_pool_status_lists[thread_id];
    zero_status_lists(to_return);
    return to_return;
  }
  return NULL;
}

/*
 * Add a new offline source to an SacnSourceStatusLists.
 *
 * [out] lists Status lists instance to which to append the new source.
 * [in] handle Handle of the offline source.
 * [in] name Name of the offline source - just a reference to the name buffer stored with the
 *           corresponding SacnTrackedSource.
 * [in] terminated Whether the source was lost because its Stream_Terminated bit was set.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_offline_source(SacnSourceStatusLists* lists, sacn_remote_source_t handle, const char* name, bool terminated)
{
  SACN_ASSERT(lists);

  CHECK_ROOM_FOR_ONE_MORE(lists, offline, SacnLostSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

  lists->offline[lists->num_offline].handle = handle;
  lists->offline[lists->num_offline].name = name;
  lists->offline[lists->num_offline].terminated = terminated;
  ++lists->num_offline;
  return true;
}

/*
 * Add a new online source to an SacnSourceStatusLists.
 *
 * [out] lists Status lists instance to which to append the new source.
 * [in] handle Handle of the online source.
 * [in] name Name of the online source - just a reference to the name buffer stored with the
 *           corresponding SacnTrackedSource.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_online_source(SacnSourceStatusLists* lists, sacn_remote_source_t handle, const char* name)
{
  SACN_ASSERT(lists);

  CHECK_ROOM_FOR_ONE_MORE(lists, online, SacnRemoteSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

  lists->online[lists->num_online].handle = handle;
  lists->online[lists->num_online].name = name;
  ++lists->num_online;
  return true;
}

/*
 * Add a new unknown-status source to an SacnSourceStatusLists.
 *
 * [out] lists Status lists instance to which to append the new source.
 * [in] handle Handle of the unknown-status source.
 * [in] name Name of the unknown-status source - just a reference to the name buffer stored with
 *           the corresponding SacnTrackedSource.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_unknown_source(SacnSourceStatusLists* lists, sacn_remote_source_t handle, const char* name)
{
  SACN_ASSERT(lists);

  CHECK_ROOM_FOR_ONE_MORE(lists, unknown, SacnRemoteSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

  lists->unknown[lists->num_unknown].handle = handle;
  lists->unknown[lists->num_unknown].name = name;
  ++lists->num_unknown;
  return true;
}

void zero_status_lists(SacnSourceStatusLists* lists)
{
  SACN_ASSERT(lists);

  lists->num_online = 0;
  lists->num_offline = 0;
  lists->num_unknown = 0;
}

#if SACN_DYNAMIC_MEM

etcpal_error_t init_status_lists_buf(unsigned int num_threads)
{
  sacn_pool_status_lists = calloc(num_threads, sizeof(SacnSourceStatusLists));
  if (!sacn_pool_status_lists)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_status_lists_entry(&sacn_pool_status_lists[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_status_lists_entry(SacnSourceStatusLists* lists)
{
  SACN_ASSERT(lists);

  etcpal_error_t res = kEtcPalErrOk;

  lists->offline = calloc(INITIAL_CAPACITY, sizeof(SacnLostSourceInternal));
  if (lists->offline)
    lists->offline_capacity = INITIAL_CAPACITY;
  else
    res = kEtcPalErrNoMem;

  if (res == kEtcPalErrOk)
  {
    lists->online = calloc(INITIAL_CAPACITY, sizeof(SacnRemoteSourceInternal));
    if (lists->online)
      lists->online_capacity = INITIAL_CAPACITY;
    else
      res = kEtcPalErrNoMem;
  }

  if (res == kEtcPalErrOk)
  {
    lists->unknown = calloc(INITIAL_CAPACITY, sizeof(SacnRemoteSourceInternal));
    if (lists->unknown)
      lists->unknown_capacity = INITIAL_CAPACITY;
    else
      res = kEtcPalErrNoMem;
  }

  return res;
}

void deinit_status_lists_buf(void)
{
  if (sacn_pool_status_lists)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_status_lists_entry(&sacn_pool_status_lists[i]);
    free(sacn_pool_status_lists);
    sacn_pool_status_lists = NULL;
  }
}

void deinit_status_lists_entry(SacnSourceStatusLists* lists)
{
  SACN_ASSERT(lists);

  CLEAR_BUF(lists, offline);
  CLEAR_BUF(lists, online);
  CLEAR_BUF(lists, unknown);
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
