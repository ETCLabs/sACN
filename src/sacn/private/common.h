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

/*
 * sacn/private/common.h: Definitions used throughout the sACN library.
 *
 * Lots of type definitions are here because they're used in multiple other places, mostly because
 * they are used by the memory manager as well as in their respective modules.
 */

#ifndef SACN_PRIVATE_COMMON_H_
#define SACN_PRIVATE_COMMON_H_

#include <limits.h>
#include "etcpal/common.h"
#include "etcpal/lock.h"
#include "etcpal/log.h"
#include "etcpal/rbtree.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "sacn/receiver.h"
#include "sacn/source.h"
#include "sacn/dmx_merger.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Global constants, macros, types, etc.
 *****************************************************************************/

#define SACN_MTU 1472
#define SACN_PORT 5568

#define SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE 512
#define SACN_DISCOVERY_UNIVERSE 64214
#define SACN_UNIVERSE_DISCOVERY_INTERVAL 10000

/*
 * This ensures there are always enough SocketRefs. This is multiplied by 2 because SocketRefs come in pairs - one for
 * IPv4, and another for IPv6. This is because a single SocketRef cannot intermix IPv4 and IPv6.
 */
#define SACN_RECEIVER_MAX_SOCKET_REFS \
  ((((SACN_RECEIVER_MAX_UNIVERSES - 1) / SACN_RECEIVER_MAX_SUBS_PER_SOCKET) + 1) * 2)

typedef unsigned int sacn_thread_id_t;
#define SACN_THREAD_ID_INVALID UINT_MAX

#define UNIVERSE_ID_VALID(universe_id) ((universe_id != 0) && (universe_id < 64000))

#define SACN_SOURCE_ENABLED                                                                              \
  ((!SACN_DYNAMIC_MEM && (SACN_SOURCE_MAX_SOURCES > 0) && (SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE > 0)) || \
   SACN_DYNAMIC_MEM)
#define SACN_SOURCE_UNICAST_ENABLED \
  ((!SACN_DYNAMIC_MEM && (SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE > 0)) || SACN_DYNAMIC_MEM)

/*
 * The SACN_DECLARE_BUF() macro declares one of two different types of contiguous arrays, depending
 * on the value of SACN_DYNAMIC_MEM.
 *
 * Given an invocation with type Foo, name foo, max_static_size 10:
 *
 * - If SACN_DYNAMIC_MEM=1, it will make the declaration:
 *
 *   Foo *foo;
 *   size_t foo_capacity;
 *
 *   max_static_size will be ignored. This pointer must then be initialized using malloc() and the
 *   capacity member is used to track how long the malloc'd array is.
 *
 * - If SACN_DYNAMIC_MEM=0, it will make the declaration:
 *
 *   Foo foo[10];
 */
#if SACN_DYNAMIC_MEM
#define SACN_DECLARE_BUF(type, name, max_static_size) \
  type* name;                                         \
  size_t name##_capacity
#else
#define SACN_DECLARE_BUF(type, name, max_static_size) type name[max_static_size]
#endif

/******************************************************************************
 * Logging
 *****************************************************************************/

#if SACN_LOGGING_ENABLED
#define SACN_LOG(pri, ...) etcpal_log(sacn_log_params, (pri), SACN_LOG_MSG_PREFIX __VA_ARGS__)
#define SACN_LOG_EMERG(...) SACN_LOG(ETCPAL_LOG_EMERG, __VA_ARGS__)
#define SACN_LOG_ALERT(...) SACN_LOG(ETCPAL_LOG_ALERT, __VA_ARGS__)
#define SACN_LOG_CRIT(...) SACN_LOG(ETCPAL_LOG_CRIT, __VA_ARGS__)
#define SACN_LOG_ERR(...) SACN_LOG(ETCPAL_LOG_ERR, __VA_ARGS__)
#define SACN_LOG_WARNING(...) SACN_LOG(ETCPAL_LOG_WARNING, __VA_ARGS__)
#define SACN_LOG_NOTICE(...) SACN_LOG(ETCPAL_LOG_NOTICE, __VA_ARGS__)
#define SACN_LOG_INFO(...) SACN_LOG(ETCPAL_LOG_INFO, __VA_ARGS__)
#define SACN_LOG_DEBUG(...) SACN_LOG(ETCPAL_LOG_DEBUG, __VA_ARGS__)

#define SACN_CAN_LOG(pri) etcpal_can_log(sacn_log_params, (pri))
#else
#define SACN_LOG(pri, ...)
#define SACN_LOG_EMERG(...)
#define SACN_LOG_ALERT(...)
#define SACN_LOG_CRIT(...)
#define SACN_LOG_ERR(...)
#define SACN_LOG_WARNING(...)
#define SACN_LOG_NOTICE(...)
#define SACN_LOG_INFO(...)
#define SACN_LOG_DEBUG(...)

#define SACN_CAN_LOG(pri) false
#endif

/******************************************************************************
 * Common types
 *****************************************************************************/

typedef struct SacnInternalNetintArray
{
  SACN_DECLARE_BUF(EtcPalMcastNetintId, netints, SACN_MAX_NETINTS);
  size_t num_netints;
} SacnInternalNetintArray;

/******************************************************************************
 * Types used by the source loss module
 *****************************************************************************/

typedef struct SacnRemoteSourceInternal
{
  EtcPalUuid cid;
  const char* name;
} SacnRemoteSourceInternal;

typedef struct SacnLostSourceInternal
{
  EtcPalUuid cid;
  const char* name;
  bool terminated;
} SacnLostSourceInternal;

/* A set of sources that is created when a source goes offline. If additional sources go offline in
 * the same time window, they are passed to the application as a set. */
typedef struct TerminationSet TerminationSet;
struct TerminationSet
{
  EtcPalTimer wait_period;
  EtcPalRbTree sources;
  TerminationSet* next;
};

/* A source in a termination set. Sources are removed from the termination set as they are
 * determined to be online. */
typedef struct TerminationSetSource
{
  EtcPalUuid cid;  // Must remain the first element in the struct for red-black tree lookup.
  const char* name;
  bool offline;
  bool terminated;
} TerminationSetSource;

/******************************************************************************
 * Types used by the sACN Receive module
 *****************************************************************************/

/* The keys that are used to look up receivers in the binary trees, for ease of comparison */
typedef struct SacnReceiverKeys
{
  sacn_receiver_t handle;
  uint16_t universe;
} SacnReceiverKeys;

/* An sACN universe to which we are currently listening. */
typedef struct SacnReceiver SacnReceiver;
struct SacnReceiver
{
  // Identification
  SacnReceiverKeys keys;
  sacn_thread_id_t thread_id;

  // Sockets / network interface info
  etcpal_socket_t ipv4_socket;
  etcpal_socket_t ipv6_socket;
  /* Array of network interfaces on which to listen to the specified universe. */
  SacnInternalNetintArray netints;

  // State tracking
  bool sampling;
  bool notified_sampling_started;
  EtcPalTimer sample_timer;
  bool suppress_limit_exceeded_notification;
  EtcPalRbTree sources;       // The sources being tracked on this universe.
  TerminationSet* term_sets;  // Source loss tracking

  // Option flags
  bool filter_preview_data;

  // Configured callbacks
  SacnReceiverCallbacks callbacks;

  /* The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
   * This parameter is ignored when configured to use static memory -- #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used
   * instead. */
  size_t source_count_max;

  /* What IP networking the receiver will support. */
  sacn_ip_support_t ip_supported;

  SacnReceiver* next;
};

/* A set of linked lists to track the state of sources in the tick function. */
typedef struct SacnSourceStatusLists
{
  SACN_DECLARE_BUF(SacnLostSourceInternal, offline, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_offline;
  SACN_DECLARE_BUF(SacnRemoteSourceInternal, online, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_online;
  SACN_DECLARE_BUF(SacnRemoteSourceInternal, unknown, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_unknown;
} SacnSourceStatusLists;

#if SACN_ETC_PRIORITY_EXTENSION
typedef enum
{
  kRecvStateWaitingForDmx,
  kRecvStateWaitingForPap,
  kRecvStateHaveDmxOnly,
  kRecvStateHaveDmxAndPap
} sacn_recv_state_t;
#endif

/* An sACN source that is being tracked on a given universe. */
typedef struct SacnTrackedSource
{
  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];
  EtcPalTimer packet_timer;
  uint8_t seq;
  bool terminated;
  bool dmx_received_since_last_tick;

#if SACN_ETC_PRIORITY_EXTENSION
  sacn_recv_state_t recv_state;
  /* pap stands for Per-Address Priority. */
  EtcPalTimer pap_timer;
#endif
} SacnTrackedSource;

typedef enum
{
  kCloseSocketNow,
  kQueueSocketForClose
} socket_close_behavior_t;

/******************************************************************************
 * Notifications delivered by the sACN receive module
 *****************************************************************************/

/* Data for the universe_data() callback */
typedef struct UniverseDataNotification
{
  SacnUniverseDataCallback callback;
  sacn_receiver_t handle;
  uint16_t universe;
  bool is_sampling;
  SacnHeaderData header;
  const uint8_t* pdata;
  void* context;
} UniverseDataNotification;

/* Data for the sources_lost() callback */
typedef struct SourcesLostNotification
{
  SacnSourcesLostCallback callback;
  sacn_receiver_t handle;
  uint16_t universe;
  SACN_DECLARE_BUF(SacnLostSource, lost_sources, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_lost_sources;
  void* context;
} SourcesLostNotification;

/* Data for the sampling_period_started() callback */
typedef struct SamplingStartedNotification
{
  SacnSamplingPeriodStartedCallback callback;
  sacn_receiver_t handle;
  uint16_t universe;
  void* context;
} SamplingStartedNotification;

/* Data for the sampling_period_ended() callback */
typedef struct SamplingEndedNotification
{
  SacnSamplingPeriodEndedCallback callback;
  sacn_receiver_t handle;
  uint16_t universe;
  void* context;
} SamplingEndedNotification;

/* Data for the source_pap_lost() callback */
typedef struct SourcePapLostNotification
{
  SacnSourcePapLostCallback callback;
  SacnRemoteSource source;
  sacn_receiver_t handle;
  uint16_t universe;
  void* context;
} SourcePapLostNotification;

/* Data for the source_limit_exceeded() callback */
typedef struct SourceLimitExceededNotification
{
  SacnSourceLimitExceededCallback callback;
  sacn_receiver_t handle;
  uint16_t universe;
  void* context;
} SourceLimitExceededNotification;

/* For the shared-socket model, this represents a shared socket. */
typedef struct SocketRef
{
  etcpal_socket_t sock;    /* The socket descriptor. */
  size_t refcount;         /* How many addresses the socket is subscribed to. */
  etcpal_iptype_t ip_type; /* The IP type used in multicast subscriptions and the bind address. */
#if SACN_RECEIVER_LIMIT_BIND
  bool bound; /* True if bind was called on this socket, false otherwise. */
#endif
} SocketRef;

/* Holds the discrete data used by each receiver thread. */
typedef struct SacnRecvThreadContext
{
  sacn_thread_id_t thread_id;
  etcpal_thread_t thread_handle;
  bool running;

  SacnReceiver* receivers;
  size_t num_receivers;

  // We do most interactions with sockets from the same thread that we receive from them, to avoid
  // thread safety foibles on some platforms. So, sockets to add and remove from the thread's
  // polling context are queued to be acted on from the thread.
  SACN_DECLARE_BUF(etcpal_socket_t, dead_sockets, SACN_RECEIVER_MAX_UNIVERSES * 2);
  size_t num_dead_sockets;

  SACN_DECLARE_BUF(SocketRef, socket_refs, SACN_RECEIVER_MAX_SOCKET_REFS);
  size_t num_socket_refs;
  size_t new_socket_refs;
#if SACN_RECEIVER_LIMIT_BIND
  bool ipv4_bound;
  bool ipv6_bound;
#endif

  // This section is only touched from the thread, outside the lock.
  EtcPalPollContext poll_context;
  uint8_t recv_buf[SACN_MTU];
} SacnRecvThreadContext;

/******************************************************************************
 * Types used by the sACN Source module
 *****************************************************************************/

typedef struct SacnSourceNetint
{
  EtcPalMcastNetintId id;  // This must be the first struct member.
  size_t num_refs;         // Number of universes using this netint.
} SacnSourceNetint;

typedef struct SacnUnicastDestination
{
  EtcPalIpAddr dest_addr;  // This must be the first struct member.
  bool terminating;
  int num_terminations_sent;
} SacnUnicastDestination;

typedef struct SacnSourceUniverse SacnSourceUniverse;
struct SacnSourceUniverse
{
  uint16_t universe_id;  // This must be the first struct member.

  bool terminating;
  int num_terminations_sent;

  uint8_t priority;
  uint16_t sync_universe;
  bool send_preview;
  uint8_t seq_num;

  // Start code 0x00 state
  int level_packets_sent_before_suppression;
  EtcPalTimer level_keep_alive_timer;
  uint8_t level_send_buf[SACN_MTU];
  bool has_level_data;

#if SACN_ETC_PRIORITY_EXTENSION
  // Start code 0xDD state
  int pap_packets_sent_before_suppression;
  EtcPalTimer pap_keep_alive_timer;
  uint8_t pap_send_buf[SACN_MTU];
  bool has_pap_data;
#endif

  SACN_DECLARE_BUF(SacnUnicastDestination, unicast_dests, SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE);
  size_t num_unicast_dests;
  bool send_unicast_only;

  SacnInternalNetintArray netints;
};

typedef struct SacnSource SacnSource;
struct SacnSource
{
  sacn_source_t handle;  // This must be the first struct member.

  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];

  bool terminating;  // If in the process of terminating all universes and removing this source.

  SACN_DECLARE_BUF(SacnSourceUniverse, universes, SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE);
  size_t num_universes;
  size_t num_active_universes;  // Number of universes to include in universe discovery packets.
  EtcPalTimer universe_discovery_timer;
  bool process_manually;
  sacn_ip_support_t ip_supported;
  int keep_alive_interval;
  size_t universe_count_max;

  // This is the set of unique netints used by all universes of this source, to be used when transmitting universe
  // discovery packets.
  SACN_DECLARE_BUF(SacnSourceNetint, netints, SACN_MAX_NETINTS);
  size_t num_netints;

  uint8_t universe_discovery_send_buf[SACN_MTU];
};

typedef enum
{
  kEnableForceSync,
  kDisableForceSync
} force_sync_behavior_t;

/******************************************************************************
 * Global variables, functions, and state tracking
 *****************************************************************************/

extern const EtcPalLogParams* sacn_log_params;

bool sacn_lock(void);
void sacn_unlock(void);

bool sacn_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_COMMON_H_ */
