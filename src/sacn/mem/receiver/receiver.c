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

#include "sacn/private/mem/receiver/receiver.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/sockets.h"
#include "sacn/private/mem/common.h"
#include "sacn/private/mem/receiver/remote_source.h"
#include "sacn/private/mem/receiver/tracked_source.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_RECEIVER_ENABLED

/**************************** Private constants ******************************/

#define SACN_RECEIVER_MAX_RB_NODES (SACN_RECEIVER_MAX_UNIVERSES * 2)

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_RECEIVER() malloc(sizeof(SacnReceiver))
#define FREE_RECEIVER(ptr)        \
  do                              \
  {                               \
    if (ptr->netints.netints)     \
    {                             \
      free(ptr->netints.netints); \
    }                             \
    free(ptr);                    \
  } while (0)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_RECEIVER() etcpal_mempool_alloc(sacn_pool_recv_receivers)
#define FREE_RECEIVER(ptr) etcpal_mempool_free(sacn_pool_recv_receivers, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_receivers, SacnReceiver, SACN_RECEIVER_MAX_UNIVERSES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_rb_nodes, EtcPalRbNode, SACN_RECEIVER_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

static EtcPalRbTree receivers;
static EtcPalRbTree receivers_by_universe;

/*********************** Private function prototypes *************************/

// Receiver memory management
static etcpal_error_t insert_receiver_into_maps(SacnReceiver* receiver);
static void remove_receiver_from_maps(SacnReceiver* receiver);

// Receiver tree node management
static int receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare_by_universe(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* receiver_node_alloc(void);
static void receiver_node_dealloc(EtcPalRbNode* node);
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

/*************************** Function definitions ****************************/

/*
 * Allocate a new receiver instances and do essential first initialization, in preparation for
 * creating the sockets and subscriptions.
 *
 * [in] handle Handle to use for this receiver.
 * [in] config Receiver configuration data.
 * [in] netint_config Network interface list for the receiver to use.
 * [in] internal_callbacks If NULL, callbacks from config are used. Otherwise these are called instead.
 * [out] receiver_state The new initialized receiver instance, or NULL if out of memory.
 * Returns an error or kEtcPalErrOk.
 */
etcpal_error_t add_sacn_receiver(sacn_receiver_t handle, const SacnReceiverConfig* config,
                                 const SacnNetintConfig* netint_config,
                                 const SacnReceiverInternalCallbacks* internal_callbacks, SacnReceiver** receiver_state)
{
  SACN_ASSERT(config);

  // First check to see if we are already listening on this universe.
  SacnReceiver* tmp = NULL;
  if (lookup_receiver_by_universe(config->universe_id, &tmp) == kEtcPalErrOk)
    return kEtcPalErrExists;

  if (handle == SACN_RECEIVER_INVALID)
    return kEtcPalErrNoMem;

  SacnReceiver* receiver = ALLOC_RECEIVER();
  if (!receiver)
    return kEtcPalErrNoMem;

  receiver->keys.handle = handle;
  receiver->keys.universe = config->universe_id;
  receiver->thread_id = SACN_THREAD_ID_INVALID;

  receiver->ipv4_socket = ETCPAL_SOCKET_INVALID;
  receiver->ipv6_socket = ETCPAL_SOCKET_INVALID;

#if SACN_DYNAMIC_MEM
  receiver->netints.netints = NULL;
  receiver->netints.netints_capacity = 0;
#endif
  receiver->netints.num_netints = 0;

  etcpal_error_t initialize_receiver_netints_result =
      sacn_initialize_receiver_netints(&receiver->netints, netint_config);
  if (initialize_receiver_netints_result != kEtcPalErrOk)
  {
    FREE_RECEIVER(receiver);
    return initialize_receiver_netints_result;
  }

  receiver->sampling = false;
  receiver->notified_sampling_started = false;
  receiver->suppress_limit_exceeded_notification = false;
  etcpal_rbtree_init(&receiver->sources, remote_source_compare, tracked_source_node_alloc, tracked_source_node_dealloc);
  receiver->term_sets = NULL;

  receiver->filter_preview_data = ((config->flags & SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA) != 0);

  receiver->api_callbacks = config->callbacks;

  if (internal_callbacks)
  {
    receiver->internal_callbacks = *internal_callbacks;
  }
  else
  {
    receiver->internal_callbacks.universe_data = NULL;
    receiver->internal_callbacks.sources_lost = NULL;
    receiver->internal_callbacks.sampling_period_started = NULL;
    receiver->internal_callbacks.sampling_period_ended = NULL;
    receiver->internal_callbacks.source_pap_lost = NULL;
    receiver->internal_callbacks.source_limit_exceeded = NULL;
  }

  receiver->source_count_max = config->source_count_max;

  receiver->ip_supported = config->ip_supported;

  receiver->next = NULL;

  *receiver_state = receiver;

  // Insert the new universe into the map.
  return insert_receiver_into_maps(receiver);
}

etcpal_error_t lookup_receiver(sacn_receiver_t handle, SacnReceiver** receiver_state)
{
  *receiver_state = (SacnReceiver*)etcpal_rbtree_find(&receivers, &handle);
  return (*receiver_state) ? kEtcPalErrOk : kEtcPalErrNotFound;
}

etcpal_error_t lookup_receiver_by_universe(uint16_t universe, SacnReceiver** receiver_state)
{
  SacnReceiverKeys lookup_keys;
  lookup_keys.universe = universe;
  *receiver_state = (SacnReceiver*)etcpal_rbtree_find(&receivers_by_universe, &lookup_keys);

  return (*receiver_state) ? kEtcPalErrOk : kEtcPalErrNotFound;
}

SacnReceiver* get_first_receiver(EtcPalRbIter* iterator)
{
  etcpal_rbiter_init(iterator);
  return (SacnReceiver*)etcpal_rbiter_first(iterator, &receivers);
}

SacnReceiver* get_next_receiver(EtcPalRbIter* iterator)
{
  return (SacnReceiver*)etcpal_rbiter_next(iterator);
}

etcpal_error_t update_receiver_universe(SacnReceiver* receiver, uint16_t new_universe)
{
  etcpal_error_t res = etcpal_rbtree_remove(&receivers_by_universe, receiver);

  if (res == kEtcPalErrOk)
  {
    receiver->keys.universe = new_universe;
    res = etcpal_rbtree_insert(&receivers_by_universe, receiver);
  }

  return res;
}

void remove_sacn_receiver(SacnReceiver* receiver)
{
  etcpal_rbtree_clear_with_cb(&receiver->sources, tracked_source_tree_dealloc);
  remove_receiver_from_maps(receiver);
  FREE_RECEIVER(receiver);
}

/*
 * Add a receiver to the maps that are used to track receivers globally.
 *
 * [in] receiver Receiver instance to add.
 * Returns error code indicating the result of the operations.
 */
etcpal_error_t insert_receiver_into_maps(SacnReceiver* receiver)
{
  etcpal_error_t res = etcpal_rbtree_insert(&receivers, receiver);
  if (res == kEtcPalErrOk)
  {
    res = etcpal_rbtree_insert(&receivers_by_universe, receiver);
    if (res != kEtcPalErrOk)
      etcpal_rbtree_remove(&receivers, receiver);
  }
  return res;
}

/*
 * Remove a receiver instance from the maps that are used to track receivers globally.
 *
 * [in] receiver Receiver to remove.
 */
void remove_receiver_from_maps(SacnReceiver* receiver)
{
  etcpal_rbtree_remove(&receivers_by_universe, receiver);
  etcpal_rbtree_remove(&receivers, receiver);
}

int receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnReceiver* a = (const SacnReceiver*)value_a;
  const SacnReceiver* b = (const SacnReceiver*)value_b;
  return (a->keys.handle > b->keys.handle) - (a->keys.handle < b->keys.handle);
}

int receiver_compare_by_universe(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnReceiver* a = (const SacnReceiver*)value_a;
  const SacnReceiver* b = (const SacnReceiver*)value_b;
  return (a->keys.universe > b->keys.universe) - (a->keys.universe < b->keys.universe);
}

EtcPalRbNode* receiver_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_recv_rb_nodes);
#endif
}

void receiver_node_dealloc(EtcPalRbNode* node)
{
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_recv_rb_nodes, node);
#endif
}

/* Helper function for clearing an EtcPalRbTree containing SacnReceivers. */
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  SacnReceiver* receiver = (SacnReceiver*)node->value;
  etcpal_rbtree_clear_with_cb(&receiver->sources, tracked_source_tree_dealloc);
  CLEAR_BUF(&receiver->netints, netints);
  FREE_RECEIVER(receiver);
  receiver_node_dealloc(node);
}

etcpal_error_t init_receivers(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_recv_receivers);
  res |= etcpal_mempool_init(sacn_pool_recv_rb_nodes);
#endif  // !SACN_DYNAMIC_MEM

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&receivers, receiver_compare, receiver_node_alloc, receiver_node_dealloc);
    etcpal_rbtree_init(&receivers_by_universe, receiver_compare_by_universe, receiver_node_alloc,
                       receiver_node_dealloc);
  }

  return res;
}

void deinit_receivers(void)
{
  etcpal_rbtree_clear_with_cb(&receivers, universe_tree_dealloc);
  etcpal_rbtree_clear(&receivers_by_universe);
}

#endif  // SACN_RECEIVER_ENABLED
