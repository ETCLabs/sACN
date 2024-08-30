/******************************************************************************
 * Copyright 2024 ETC Inc.
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

#include "sacn/private/mem/merge_receiver/merge_receiver_source.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/opts.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_MERGE_RECEIVER_ENABLED || DOXYGEN

/**************************** Private constants ******************************/

#define SACN_MERGE_RECEIVER_SOURCE_MAX_RB_NODES SACN_RECEIVER_TOTAL_MAX_SOURCES

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_MERGE_RECEIVER_SOURCE()   malloc(sizeof(SacnMergeReceiverInternalSource))
#define FREE_MERGE_RECEIVER_SOURCE(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_MERGE_RECEIVER_SOURCE()   etcpal_mempool_alloc(sacn_pool_mergerecv_sources)
#define FREE_MERGE_RECEIVER_SOURCE(ptr) etcpal_mempool_free(sacn_pool_mergerecv_sources, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_mergerecv_sources, SacnMergeReceiverInternalSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_mergerecv_source_rb_nodes, EtcPalRbNode, SACN_MERGE_RECEIVER_SOURCE_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

/*********************** Private function prototypes *************************/

// Merge receiver tree node management
static void merge_receiver_sources_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

/*************************** Function definitions ****************************/

etcpal_error_t init_merge_receiver_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_mergerecv_sources);
  res |= etcpal_mempool_init(sacn_pool_mergerecv_source_rb_nodes);
#endif  // !SACN_DYNAMIC_MEM

  return res;
}

// Needs lock
etcpal_error_t add_sacn_merge_receiver_source(SacnMergeReceiver*          merge_receiver,
                                              const EtcPalSockAddr*       addr,
                                              const SacnRemoteSource*     remote_source,
                                              bool                        sampling,
                                              const SacnRecvUniverseData* universe_data)
{
  if (!SACN_ASSERT_VERIFY(merge_receiver) || !SACN_ASSERT_VERIFY(addr) || !SACN_ASSERT_VERIFY(remote_source) ||
      !SACN_ASSERT_VERIFY(universe_data))
  {
    return kEtcPalErrSys;
  }

  etcpal_error_t result = kEtcPalErrNoMem;

  SacnMergeReceiverInternalSource* src = ALLOC_MERGE_RECEIVER_SOURCE();
  if (src)
  {
    src->handle   = remote_source->handle;
    src->sampling = sampling;

    // The call below will set these to true if their respective start code is detected
    src->per_address_priorities_active = false;
    src->levels_active                 = false;

    update_merge_receiver_source_info(src, addr, remote_source, universe_data);

    result = etcpal_rbtree_insert(&merge_receiver->sources, src);

    if (result != kEtcPalErrOk)
      FREE_MERGE_RECEIVER_SOURCE(src);
  }

  return result;
}

// Needs lock
etcpal_error_t lookup_merge_receiver_source(SacnMergeReceiver*                merge_receiver,
                                            sacn_remote_source_t              source_handle,
                                            SacnMergeReceiverInternalSource** source)
{
  if (!SACN_ASSERT_VERIFY(merge_receiver) || !SACN_ASSERT_VERIFY(source_handle != kSacnRemoteSourceInvalid) ||
      !SACN_ASSERT_VERIFY(source))
  {
    return kEtcPalErrSys;
  }

  (*source) = etcpal_rbtree_find(&merge_receiver->sources, &source_handle);
  return (*source) ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
void remove_sacn_merge_receiver_source(SacnMergeReceiver* merge_receiver, sacn_remote_source_t source_handle)
{
  if (!SACN_ASSERT_VERIFY(merge_receiver) || !SACN_ASSERT_VERIFY(source_handle != kSacnRemoteSourceInvalid))
    return;

  SacnMergeReceiverInternalSource* source = etcpal_rbtree_find(&merge_receiver->sources, &source_handle);
  if (SACN_ASSERT_VERIFY(source))
    etcpal_rbtree_remove_with_cb(&merge_receiver->sources, source, merge_receiver_sources_tree_dealloc);
}

// Needs lock
void clear_sacn_merge_receiver_sources(SacnMergeReceiver* merge_receiver)
{
  if (!SACN_ASSERT_VERIFY(merge_receiver))
    return;

  etcpal_rbtree_clear_with_cb(&merge_receiver->sources, merge_receiver_sources_tree_dealloc);
}

// Needs lock
// This function is called when a source is added and when a packet is received.
void update_merge_receiver_source_info(SacnMergeReceiverInternalSource* info,
                                       const EtcPalSockAddr*            addr,
                                       const SacnRemoteSource*          remote_source,
                                       const SacnRecvUniverseData*      universe_data)
{
  if (!SACN_ASSERT_VERIFY(info) || !SACN_ASSERT_VERIFY(addr) || !SACN_ASSERT_VERIFY(remote_source) ||
      !SACN_ASSERT_VERIFY(universe_data))
  {
    return;
  }

  memcpy(info->name, remote_source->name, kSacnSourceNameMaxLen);
  info->addr = *addr;

  if (universe_data->start_code == kSacnStartcodePriority)
    info->per_address_priorities_active = true;  // Only sets to true if PAP is present - PAP lost handler sets to false
  else if (universe_data->start_code == kSacnStartcodeDmx)
    info->levels_active = true;

  info->universe_priority = universe_data->priority;
}

EtcPalRbNode* merge_receiver_source_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_mergerecv_source_rb_nodes);
#endif
}

void merge_receiver_source_node_dealloc(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_mergerecv_source_rb_nodes, node);
#endif
}

void merge_receiver_sources_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_MERGE_RECEIVER_SOURCE(node->value);
  merge_receiver_source_node_dealloc(node);
}

#endif  // SACN_MERGE_RECEIVER_ENABLED || DOXYGEN
