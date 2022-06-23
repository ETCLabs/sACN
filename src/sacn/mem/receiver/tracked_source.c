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

#include "sacn/private/mem/receiver/tracked_source.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"
#include "sacn/private/mem/receiver/remote_source.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_RECEIVER_ENABLED

/**************************** Private constants ******************************/

#define SACN_TRACKED_SOURCE_MAX_RB_NODES SACN_RECEIVER_TOTAL_MAX_SOURCES

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_TRACKED_SOURCE() malloc(sizeof(SacnTrackedSource))
#define FREE_TRACKED_SOURCE(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_TRACKED_SOURCE() etcpal_mempool_alloc(sacn_pool_recv_tracked_sources)
#define FREE_TRACKED_SOURCE(ptr) etcpal_mempool_free(sacn_pool_recv_tracked_sources, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_tracked_sources, SacnTrackedSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_recv_tracked_source_rb_nodes, EtcPalRbNode, SACN_TRACKED_SOURCE_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

/*********************** Private function prototypes *************************/

// Tracked source tree node management

/*************************** Function definitions ****************************/

etcpal_error_t init_tracked_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_recv_tracked_sources);
  res |= etcpal_mempool_init(sacn_pool_recv_tracked_source_rb_nodes);
#endif  // !SACN_DYNAMIC_MEM

  return res;
}

etcpal_error_t add_sacn_tracked_source(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const char* name,
                                       uint8_t seq_num, uint8_t first_start_code,
                                       SacnTrackedSource** tracked_source_state)
{
#if !SACN_ETC_PRIORITY_EXTENSION
  ETCPAL_UNUSED_ARG(first_start_code);
#endif

  etcpal_error_t result = kEtcPalErrOk;
  SacnTrackedSource* src = NULL;

  size_t current_number_of_sources = etcpal_rbtree_size(&receiver->sources);
#if SACN_DYNAMIC_MEM
  size_t max_number_of_sources = receiver->source_count_max;
  bool infinite_sources = (max_number_of_sources == SACN_RECEIVER_INFINITE_SOURCES);
#else
  size_t max_number_of_sources = ((receiver->source_count_max > SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE) ||
                                  (receiver->source_count_max == SACN_RECEIVER_INFINITE_SOURCES))
                                     ? SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE
                                     : receiver->source_count_max;
  bool infinite_sources = false;
#endif

  if (infinite_sources || (current_number_of_sources < max_number_of_sources))
    src = ALLOC_TRACKED_SOURCE();

  if (!src)
    result = kEtcPalErrNoMem;

  sacn_remote_source_t handle = SACN_REMOTE_SOURCE_INVALID;
  if (result == kEtcPalErrOk)
    result = add_remote_source_handle(sender_cid, &handle);

  if (result == kEtcPalErrOk)
  {
    src->handle = handle;
    ETCPAL_MSVC_NO_DEP_WRN strcpy(src->name, name);
    etcpal_timer_start(&src->packet_timer, SACN_SOURCE_LOSS_TIMEOUT);
    src->seq = seq_num;
    src->terminated = false;
    src->dmx_received_since_last_tick = true;

#if SACN_ETC_PRIORITY_EXTENSION
    if (receiver->sampling)
    {
      if (first_start_code == SACN_STARTCODE_PRIORITY)
      {
        // Need to wait for DMX - ignore PAP packets until we've seen at least one DMX packet.
        src->recv_state = kRecvStateWaitingForDmx;
        etcpal_timer_start(&src->pap_timer, SACN_SOURCE_LOSS_TIMEOUT);
      }
      else
      {
        // If we are in the sampling period, the wait period for PAP is not necessary.
        src->recv_state = kRecvStateHaveDmxOnly;
      }
    }
    else
    {
      // Even if this is a priority packet, we want to make sure that DMX packets are also being
      // sent before notifying.
      if (first_start_code == SACN_STARTCODE_PRIORITY)
        src->recv_state = kRecvStateWaitingForDmx;
      else
        src->recv_state = kRecvStateWaitingForPap;
      etcpal_timer_start(&src->pap_timer, SACN_WAIT_FOR_PRIORITY);
    }
#endif

    result = etcpal_rbtree_insert(&receiver->sources, src);
  }

  if (result == kEtcPalErrOk)
  {
    *tracked_source_state = src;
  }
  else
  {
    if (handle != SACN_REMOTE_SOURCE_INVALID)
      remove_remote_source_handle(handle);
    if (src)
      FREE_TRACKED_SOURCE(src);
  }

  return result;
}

etcpal_error_t clear_receiver_sources(SacnReceiver* receiver)
{
  receiver->suppress_limit_exceeded_notification = false;
  return etcpal_rbtree_clear_with_cb(&receiver->sources, tracked_source_tree_dealloc);
}

etcpal_error_t remove_receiver_source(SacnReceiver* receiver, sacn_remote_source_t handle)
{
  return etcpal_rbtree_remove_with_cb(&receiver->sources, &handle, tracked_source_tree_dealloc);
}

/* Helper function for clearing an EtcPalRbTree containing sources. */
void tracked_source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  sacn_remote_source_t* handle = (sacn_remote_source_t*)node->value;
  remove_remote_source_handle(*handle);

  FREE_TRACKED_SOURCE(node->value);
  tracked_source_node_dealloc(node);
}

void tracked_source_node_dealloc(EtcPalRbNode* node)
{
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_recv_tracked_source_rb_nodes, node);
#endif
}

EtcPalRbNode* tracked_source_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_recv_tracked_source_rb_nodes);
#endif
}

#endif  // SACN_RECEIVER_ENABLED
