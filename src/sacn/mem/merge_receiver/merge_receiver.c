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

#include "sacn/private/mem/merge_receiver/merge_receiver.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"
#include "sacn/private/mem/receiver/remote_source.h"
#include "sacn/private/mem/merge_receiver/merge_receiver_source.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_MERGE_RECEIVER_ENABLED

/**************************** Private constants ******************************/

#if SACN_DMX_MERGER_MAX_MERGERS < SACN_RECEIVER_MAX_UNIVERSES
#define MAX_MERGE_RECEIVERS SACN_DMX_MERGER_MAX_MERGERS
#else
#define MAX_MERGE_RECEIVERS SACN_RECEIVER_MAX_UNIVERSES
#endif

/**************************** Private variables ******************************/

static bool merge_receivers_initialized = false;

static struct SacnMergeReceiverMem
{
  SACN_DECLARE_MERGE_RECEIVER_BUF(SacnMergeReceiver, merge_receivers, MAX_MERGE_RECEIVERS);
  size_t num_merge_receivers;
} sacn_pool_merge_receiver_mem;

/*********************** Private function prototypes *************************/

static size_t get_merge_receiver_index(sacn_merge_receiver_t handle, bool* found);

/*************************** Function definitions ****************************/

// Needs lock
etcpal_error_t add_sacn_merge_receiver(sacn_merge_receiver_t handle, const SacnMergeReceiverConfig* config,
                                       SacnMergeReceiver** state)
{
  etcpal_error_t result = kEtcPalErrOk;

  SacnMergeReceiver* merge_receiver = NULL;
  if (lookup_merge_receiver(handle, &merge_receiver, NULL) == kEtcPalErrOk)
    result = kEtcPalErrExists;

  if (result == kEtcPalErrOk)
  {
    CHECK_ROOM_FOR_ONE_MORE((&sacn_pool_merge_receiver_mem), merge_receivers, SacnMergeReceiver, MAX_MERGE_RECEIVERS,
                            kEtcPalErrNoMem);

    merge_receiver = &sacn_pool_merge_receiver_mem.merge_receivers[sacn_pool_merge_receiver_mem.num_merge_receivers];

    merge_receiver->merge_receiver_handle = handle;
    merge_receiver->merger_handle = SACN_DMX_MERGER_INVALID;
    merge_receiver->callbacks = config->callbacks;
    merge_receiver->use_pap = config->use_pap;

    memset(merge_receiver->levels, 0, DMX_ADDRESS_COUNT);
    memset(merge_receiver->owners, 0, DMX_ADDRESS_COUNT * sizeof(sacn_dmx_merger_source_t));

    etcpal_rbtree_init(&merge_receiver->sources, remote_source_compare, merge_receiver_source_node_alloc,
                       merge_receiver_source_node_dealloc);

    merge_receiver->num_pending_sources = 0;
    merge_receiver->sampling = true;

    ++sacn_pool_merge_receiver_mem.num_merge_receivers;
  }

  *state = merge_receiver;

  return result;
}

// Needs lock
etcpal_error_t lookup_merge_receiver(sacn_merge_receiver_t handle, SacnMergeReceiver** state, size_t* index)
{
  bool found = false;
  size_t idx = get_merge_receiver_index(handle, &found);
  if (found)
  {
    *state = &sacn_pool_merge_receiver_mem.merge_receivers[idx];
    if (index)
      *index = idx;
  }
  else
  {
    *state = NULL;
  }

  return found ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
SacnMergeReceiver* get_merge_receiver(size_t index)
{
  return (index < sacn_pool_merge_receiver_mem.num_merge_receivers)
             ? &sacn_pool_merge_receiver_mem.merge_receivers[index]
             : NULL;
}

// Needs lock
size_t get_num_merge_receivers()
{
  return sacn_pool_merge_receiver_mem.num_merge_receivers;
}

// Needs lock
void remove_sacn_merge_receiver(size_t index)
{
  clear_sacn_merge_receiver_sources(&sacn_pool_merge_receiver_mem.merge_receivers[index]);
  REMOVE_AT_INDEX((&sacn_pool_merge_receiver_mem), SacnMergeReceiver, merge_receivers, index);
}

size_t get_merge_receiver_index(sacn_merge_receiver_t handle, bool* found)
{
  *found = false;
  size_t index = 0;

  while (!(*found) && (index < sacn_pool_merge_receiver_mem.num_merge_receivers))
  {
    if (sacn_pool_merge_receiver_mem.merge_receivers[index].merge_receiver_handle == handle)
      *found = true;
    else
      ++index;
  }

  return index;
}

etcpal_error_t init_merge_receivers(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if SACN_DYNAMIC_MEM
  sacn_pool_merge_receiver_mem.merge_receivers = calloc(INITIAL_CAPACITY, sizeof(SacnMergeReceiver));
  sacn_pool_merge_receiver_mem.merge_receivers_capacity =
      sacn_pool_merge_receiver_mem.merge_receivers ? INITIAL_CAPACITY : 0;
  if (!sacn_pool_merge_receiver_mem.merge_receivers)
    res = kEtcPalErrNoMem;
#endif
  sacn_pool_merge_receiver_mem.num_merge_receivers = 0;

  if (res == kEtcPalErrOk)
    merge_receivers_initialized = true;

  return res;
}

void deinit_merge_receivers(void)
{
  if (sacn_lock())
  {
    if (merge_receivers_initialized)
    {
      for (size_t i = 0; i < sacn_pool_merge_receiver_mem.num_merge_receivers; ++i)
        clear_sacn_merge_receiver_sources(&sacn_pool_merge_receiver_mem.merge_receivers[i]);

      CLEAR_BUF(&sacn_pool_merge_receiver_mem, merge_receivers);
#if SACN_DYNAMIC_MEM
      sacn_pool_merge_receiver_mem.merge_receivers_capacity = 0;
#endif  // SACN_DYNAMIC_MEM

      memset(&sacn_pool_merge_receiver_mem, 0, sizeof sacn_pool_merge_receiver_mem);

      merge_receivers_initialized = false;
    }

    sacn_unlock();
  }
}

#endif  // SACN_MERGE_RECEIVER_ENABLED
