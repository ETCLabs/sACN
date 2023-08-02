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

#define SACN_MERGE_RECEIVER_MAX_RB_NODES MAX_MERGE_RECEIVERS

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_MERGE_RECEIVER() malloc(sizeof(SacnMergeReceiver))
#define FREE_MERGE_RECEIVER(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_MERGE_RECEIVER() etcpal_mempool_alloc(sacn_pool_mergerecv_receivers)
#define FREE_MERGE_RECEIVER(ptr) etcpal_mempool_free(sacn_pool_mergerecv_receivers, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

static bool merge_receivers_initialized = false;

static EtcPalRbTree merge_receivers;

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_mergerecv_receivers, SacnMergeReceiver, MAX_MERGE_RECEIVERS);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_mergerecv_receiver_rb_nodes, EtcPalRbNode, SACN_MERGE_RECEIVER_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

/*********************** Private function prototypes *************************/

int merge_receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);

// Merge receiver tree node management
EtcPalRbNode* merge_receiver_node_alloc(void);
void merge_receiver_node_dealloc(EtcPalRbNode* node);
void merge_receiver_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

/*************************** Function definitions ****************************/

// Needs lock
etcpal_error_t add_sacn_merge_receiver(sacn_merge_receiver_t handle, const SacnMergeReceiverConfig* config,
                                       SacnMergeReceiver** state)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_MERGE_RECEIVER_INVALID) || !SACN_ASSERT_VERIFY(config) ||
      !SACN_ASSERT_VERIFY(state))
  {
    return kEtcPalErrSys;
  }

  etcpal_error_t result = kEtcPalErrOk;

  SacnMergeReceiver* merge_receiver = NULL;
  if (lookup_merge_receiver(handle, &merge_receiver) == kEtcPalErrOk)
    result = kEtcPalErrExists;

  if (result == kEtcPalErrOk)
  {
    merge_receiver = ALLOC_MERGE_RECEIVER();
    if (!merge_receiver)
      result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
  {
    merge_receiver->merge_receiver_handle = handle;
    merge_receiver->merger_handle = SACN_DMX_MERGER_INVALID;
    merge_receiver->callbacks = config->callbacks;
    merge_receiver->use_pap = config->use_pap;

    memset(merge_receiver->levels, 0, DMX_ADDRESS_COUNT);
    memset(merge_receiver->owners, 0, DMX_ADDRESS_COUNT * sizeof(sacn_dmx_merger_source_t));

    etcpal_rbtree_init(&merge_receiver->sources, remote_source_compare, merge_receiver_source_node_alloc,
                       merge_receiver_source_node_dealloc);

    merge_receiver->sampling = true;

    result = etcpal_rbtree_insert(&merge_receivers, merge_receiver);

    if (result != kEtcPalErrOk)
      FREE_MERGE_RECEIVER(merge_receiver);
  }

  *state = merge_receiver;

  return result;
}

// Needs lock
etcpal_error_t lookup_merge_receiver(sacn_merge_receiver_t handle, SacnMergeReceiver** state)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_MERGE_RECEIVER_INVALID) || !SACN_ASSERT_VERIFY(state))
    return kEtcPalErrSys;

  (*state) = etcpal_rbtree_find(&merge_receivers, &handle);
  return (*state) ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
size_t get_num_merge_receivers()
{
  return etcpal_rbtree_size(&merge_receivers);
}

// Needs lock
void remove_sacn_merge_receiver(sacn_merge_receiver_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_MERGE_RECEIVER_INVALID))
    return;

  SacnMergeReceiver* merge_receiver = etcpal_rbtree_find(&merge_receivers, &handle);
  if (SACN_ASSERT_VERIFY(merge_receiver))
    etcpal_rbtree_remove_with_cb(&merge_receivers, merge_receiver, merge_receiver_tree_dealloc);
}

int merge_receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnMergeReceiver* a = (const SacnMergeReceiver*)value_a;
  const SacnMergeReceiver* b = (const SacnMergeReceiver*)value_b;
  return (a->merge_receiver_handle > b->merge_receiver_handle) - (a->merge_receiver_handle < b->merge_receiver_handle);
}

EtcPalRbNode* merge_receiver_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_mergerecv_receiver_rb_nodes);
#endif
}

void merge_receiver_node_dealloc(EtcPalRbNode* node)
{
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_mergerecv_receiver_rb_nodes, node);
#endif
}

void merge_receiver_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);
  clear_sacn_merge_receiver_sources((SacnMergeReceiver*)node->value);
  FREE_MERGE_RECEIVER(node->value);
  merge_receiver_node_dealloc(node);
}

etcpal_error_t init_merge_receivers(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_mergerecv_receivers);
  res |= etcpal_mempool_init(sacn_pool_mergerecv_receiver_rb_nodes);
#endif  // !SACN_DYNAMIC_MEM

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&merge_receivers, merge_receiver_compare, merge_receiver_node_alloc,
                       merge_receiver_node_dealloc);
    merge_receivers_initialized = true;
  }

  return res;
}

void deinit_merge_receivers(void)
{
  if (sacn_lock())
  {
    if (merge_receivers_initialized)
    {
      etcpal_rbtree_clear_with_cb(&merge_receivers, merge_receiver_tree_dealloc);
      merge_receivers_initialized = false;
    }

    sacn_unlock();
  }
}

#endif  // SACN_MERGE_RECEIVER_ENABLED
