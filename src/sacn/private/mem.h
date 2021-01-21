/******************************************************************************
 * Copyright 2020 ETC Inc.
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

#ifndef SACN_PRIVATE_MEM_H_
#define SACN_PRIVATE_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "etcpal/uuid.h"
#include "sacn/receiver.h"
#include "sacn/private/common.h"
#include "sacn/private/sockets.h"
#include "sacn/private/opts.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/******************************************************************************
 * Memory constants, macros, types, etc.
 *****************************************************************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

#if SACN_DYNAMIC_MEM
#define ALLOC_SACN_SOURCE() malloc(sizeof(SacnSource))
#define FREE_SACN_SOURCE(ptr)                                                         \
  do                                                                                  \
  {                                                                                   \
    etcpal_rbtree_clear_with_cb(&((SacnSource*)ptr)->universes, free_universes_node); \
    etcpal_rbtree_clear_with_cb(&((SacnSource*)ptr)->netints, free_netints_node);     \
    free(ptr);                                                                        \
  } while (0)
#define ALLOC_SACN_SOURCE_UNIVERSE() malloc(sizeof(SacnSourceUniverse))
#define FREE_SACN_SOURCE_UNIVERSE(ptr)                                                                \
  do                                                                                                  \
  {                                                                                                   \
    etcpal_rbtree_clear_with_cb(&((SacnSourceUniverse*)ptr)->unicast_dests, free_unicast_dests_node); \
    if (((SacnSourceUniverse*)ptr)->netints)                                                          \
      free(((SacnSourceUniverse*)ptr)->netints);                                                      \
    free(ptr);                                                                                        \
  } while (0)
#define ALLOC_SACN_SOURCE_NETINT() malloc(sizeof(SacnSourceNetint))
#define FREE_SACN_SOURCE_NETINT(ptr) free(ptr)
#define ALLOC_SACN_UNICAST_DESTINATION() malloc(sizeof(SacnUnicastDestination))
#define FREE_SACN_UNICAST_DESTINATION(ptr) free(ptr)
#define ALLOC_SACN_SOURCE_RB_NODE() malloc(sizeof(EtcPalRbNode))
#define FREE_SACN_SOURCE_RB_NODE(ptr) free(ptr)
#elif SACN_SOURCE_ENABLED
#define ALLOC_SACN_SOURCE() etcpal_mempool_alloc(sacnsource_source_states)
#define FREE_SACN_SOURCE(ptr)                                                         \
  do                                                                                  \
  {                                                                                   \
    etcpal_rbtree_clear_with_cb(&((SacnSource*)ptr)->universes, free_universes_node); \
    etcpal_rbtree_clear_with_cb(&((SacnSource*)ptr)->netints, free_netints_node);     \
    etcpal_mempool_free(sacnsource_source_states, ptr);                               \
  } while (0)
#define ALLOC_SACN_SOURCE_UNIVERSE() etcpal_mempool_alloc(sacnsource_universe_states)
#define FREE_SACN_SOURCE_UNIVERSE(ptr)                                                                \
  do                                                                                                  \
  {                                                                                                   \
    etcpal_rbtree_clear_with_cb(&((SacnSourceUniverse*)ptr)->unicast_dests, free_unicast_dests_node); \
    etcpal_mempool_free(sacnsource_universe_states, ptr);                                             \
  } while (0)
#define ALLOC_SACN_SOURCE_NETINT() etcpal_mempool_alloc(sacnsource_netints)
#define FREE_SACN_SOURCE_NETINT(ptr) etcpal_mempool_free(sacnsource_netints, ptr)
#if SACN_SOURCE_UNICAST_ENABLED
#define ALLOC_SACN_UNICAST_DESTINATION() etcpal_mempool_alloc(sacnsource_unicast_dests)
#define FREE_SACN_UNICAST_DESTINATION(ptr) etcpal_mempool_free(sacnsource_unicast_dests, ptr)
#else
#define ALLOC_SACN_UNICAST_DESTINATION() NULL
#define FREE_SACN_UNICAST_DESTINATION(ptr)
#endif
#define ALLOC_SACN_SOURCE_RB_NODE() etcpal_mempool_alloc(sacnsource_rb_nodes)
#define FREE_SACN_SOURCE_RB_NODE(ptr) etcpal_mempool_free(sacnsource_rb_nodes, ptr)
#else
#define ALLOC_SACN_SOURCE() NULL
#define FREE_SACN_SOURCE(ptr)
#define ALLOC_SACN_SOURCE_UNIVERSE() NULL
#define FREE_SACN_SOURCE_UNIVERSE(ptr)
#define ALLOC_SACN_SOURCE_NETINT() NULL
#define FREE_SACN_SOURCE_NETINT(ptr)
#define ALLOC_SACN_UNICAST_DESTINATION() NULL
#define FREE_SACN_UNICAST_DESTINATION(ptr)
#define ALLOC_SACN_SOURCE_RB_NODE() NULL
#define FREE_SACN_SOURCE_RB_NODE(ptr)
#endif

/* Memory pool declarations. */

#if !SACN_DYNAMIC_MEM && SACN_SOURCE_ENABLED
ETCPAL_MEMPOOL_DECLARE(sacnsource_source_states);
ETCPAL_MEMPOOL_DECLARE(sacnsource_universe_states);
ETCPAL_MEMPOOL_DECLARE(sacnsource_netints);
#if SACN_SOURCE_UNICAST_ENABLED
ETCPAL_MEMPOOL_DECLARE(sacnsource_unicast_dests);
#endif
ETCPAL_MEMPOOL_DECLARE(sacnsource_rb_nodes);
#endif

/******************************************************************************
 * Memory functions
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_mem_init(unsigned int num_threads);
void sacn_mem_deinit(void);

unsigned int sacn_mem_get_num_threads(void);

SacnSourceStatusLists* get_status_lists(sacn_thread_id_t thread_id);
SacnTrackedSource** get_to_erase_buffer(sacn_thread_id_t thread_id, size_t size);
SacnRecvThreadContext* get_recv_thread_context(sacn_thread_id_t thread_id);

// These are processed from the context of receiving data, so there is only one per thread.
UniverseDataNotification* get_universe_data(sacn_thread_id_t thread_id);
SourcePapLostNotification* get_source_pap_lost(sacn_thread_id_t thread_id);
SourceLimitExceededNotification* get_source_limit_exceeded(sacn_thread_id_t thread_id);

// These are processed in the periodic timeout processing, so there are multiple per thread.
SourcesLostNotification* get_sources_lost_buffer(sacn_thread_id_t thread_id, size_t size);
SamplingStartedNotification* get_sampling_started_buffer(sacn_thread_id_t thread_id, size_t size);
SamplingEndedNotification* get_sampling_ended_buffer(sacn_thread_id_t thread_id, size_t size);

bool add_offline_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name, bool terminated);
bool add_online_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name);
bool add_unknown_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name);

bool add_lost_source(SourcesLostNotification* sources_lost, const EtcPalUuid* cid, const char* name, bool terminated);

bool add_dead_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket);
bool add_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket, etcpal_iptype_t ip_type,
                    bool bound);
bool remove_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket);
void add_receiver_to_list(SacnRecvThreadContext* recv_thread_context, SacnReceiver* receiver);
void remove_receiver_from_list(SacnRecvThreadContext* recv_thread_context, SacnReceiver* receiver);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MEM_H_ */
