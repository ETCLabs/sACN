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

#include "sacn/private/mem/merge_receiver/merged_data.h"

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
static MergeReceiverMergedDataNotification* sacn_pool_merged_data;
#else
static MergeReceiverMergedDataNotification sacn_pool_merged_data[SACN_RECEIVER_MAX_THREADS];
#endif

/*************************** Function definitions ****************************/

/*
 * Get the MergeReceiverMergedDataNotification instance for a given thread. The instance will be initialized
 * to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
MergeReceiverMergedDataNotification* get_merged_data(sacn_thread_id_t thread_id)
{
  if (!SACN_ASSERT_VERIFY(thread_id != SACN_THREAD_ID_INVALID))
    return NULL;

  if (thread_id < sacn_mem_get_num_threads())
  {
    MergeReceiverMergedDataNotification* to_return = &sacn_pool_merged_data[thread_id];
    to_return->callback = NULL;
    to_return->handle = SACN_MERGE_RECEIVER_INVALID;
    to_return->universe = 0;
    to_return->slot_range.start_address = 1;
    to_return->slot_range.address_count = DMX_ADDRESS_COUNT;
    memset(to_return->levels, 0, DMX_ADDRESS_COUNT);
    memset(to_return->priorities, 0, DMX_ADDRESS_COUNT);
    memset(to_return->owners, 0, DMX_ADDRESS_COUNT * sizeof(sacn_remote_source_t));
    to_return->num_active_sources = 0;

    return to_return;
  }
  return NULL;
}

/*
 * Add a merge receiver's active sources to a MergeReceiverMergedDataNotification.
 *
 * [out] notification MergeReceiverMergedDataNotification instance to which to append the active source.
 * [in] merge_receiver The merge receiver with the active sources to add to the notification.
 * Returns true if the sources were all successfully added, false if memory could not be allocated (some may have
 * already been added in this case, but not all).
 */
bool add_active_sources(MergeReceiverMergedDataNotification* notification, SacnMergeReceiver* merge_receiver)
{
  if (!SACN_ASSERT_VERIFY(notification) || !SACN_ASSERT_VERIFY(merge_receiver))
    return false;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  for (SacnMergeReceiverInternalSource* source = etcpal_rbiter_first(&iter, &merge_receiver->sources); source;
       source = etcpal_rbiter_next(&iter))
  {
    if (!source->sampling)
    {
      CHECK_ROOM_FOR_ONE_MORE(notification, active_sources, sacn_remote_source_t,
                              SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

      notification->active_sources[notification->num_active_sources] = source->handle;
      ++notification->num_active_sources;
    }
  }

  return true;
}

etcpal_error_t init_merged_data_buf(unsigned int num_threads)
{
  if (!SACN_ASSERT_VERIFY(num_threads > 0))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  sacn_pool_merged_data = calloc(num_threads, sizeof(MergeReceiverMergedDataNotification));
  if (!sacn_pool_merged_data)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    sacn_pool_merged_data[i].active_sources = calloc(INITIAL_CAPACITY, sizeof(sacn_remote_source_t));
    if (!sacn_pool_merged_data[i].active_sources)
      return kEtcPalErrNoMem;

    sacn_pool_merged_data[i].active_sources_capacity = INITIAL_CAPACITY;
  }
#else   // SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(num_threads);
  memset(sacn_pool_merged_data, 0, sizeof(sacn_pool_merged_data));
#endif  // SACN_DYNAMIC_MEM
  return kEtcPalErrOk;
}

#if SACN_DYNAMIC_MEM

void deinit_merged_data_buf(void)
{
  if (sacn_pool_merged_data)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
    {
      if (sacn_pool_merged_data[i].active_sources)
        free(sacn_pool_merged_data[i].active_sources);
    }

    free(sacn_pool_merged_data);
    sacn_pool_merged_data = NULL;
  }
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_MERGE_RECEIVER_ENABLED
