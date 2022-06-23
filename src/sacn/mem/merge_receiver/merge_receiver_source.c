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

#include "sacn/private/mem/merge_receiver/merge_receiver_source.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_MERGE_RECEIVER_ENABLED

/**************************** Private constants ******************************/

#define SACN_MERGE_RECEIVER_SOURCE_MAX_RB_NODES SACN_RECEIVER_TOTAL_MAX_SOURCES

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_MERGE_RECEIVER_SOURCE() malloc(sizeof(SacnMergeReceiverSource))
#define FREE_MERGE_RECEIVER_SOURCE(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_MERGE_RECEIVER_SOURCE() etcpal_mempool_alloc(sacn_pool_mergerecv_sources)
#define FREE_MERGE_RECEIVER_SOURCE(ptr) etcpal_mempool_free(sacn_pool_mergerecv_sources, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_mergerecv_sources, SacnMergeReceiverSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
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
etcpal_error_t add_sacn_merge_receiver_source(SacnMergeReceiver* merge_receiver, sacn_remote_source_t source_handle,
                                              bool pending)
{
  etcpal_error_t result = kEtcPalErrNoMem;

  SacnMergeReceiverSource* src = ALLOC_MERGE_RECEIVER_SOURCE();
  if (src)
  {
    src->handle = source_handle;
    src->pending = pending;

    result = etcpal_rbtree_insert(&merge_receiver->sources, src);

    if (result != kEtcPalErrOk)
      FREE_MERGE_RECEIVER_SOURCE(src);
    else if (pending)
      ++merge_receiver->num_pending_sources;
  }

  return result;
}

// Needs lock
etcpal_error_t lookup_merge_receiver_source(SacnMergeReceiver* merge_receiver, sacn_remote_source_t source_handle,
                                            SacnMergeReceiverSource** source)
{
  (*source) = etcpal_rbtree_find(&merge_receiver->sources, &source_handle);
  return (*source) ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
void remove_sacn_merge_receiver_source(SacnMergeReceiver* merge_receiver, sacn_remote_source_t source_handle)
{
  SacnMergeReceiverSource* source = etcpal_rbtree_find(&merge_receiver->sources, &source_handle);
  if (source->pending)
    --merge_receiver->num_pending_sources;

  etcpal_rbtree_remove_with_cb(&merge_receiver->sources, source, merge_receiver_sources_tree_dealloc);
}

// Needs lock
void clear_sacn_merge_receiver_sources(SacnMergeReceiver* merge_receiver)
{
  etcpal_rbtree_clear_with_cb(&merge_receiver->sources, merge_receiver_sources_tree_dealloc);
  merge_receiver->num_pending_sources = 0;
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
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_mergerecv_source_rb_nodes, node);
#endif
}

void merge_receiver_sources_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);
  FREE_MERGE_RECEIVER_SOURCE(node->value);
  merge_receiver_source_node_dealloc(node);
}

#endif  // SACN_MERGE_RECEIVER_ENABLED
