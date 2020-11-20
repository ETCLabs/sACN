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

/*
 * This ensures there are always enough SocketRefs. This is multiplied by 2 because SocketRefs come in pairs - one for
 * IPv4, and another for IPv6. This is because a single SocketRef cannot intermix IPv4 and IPv6.
 */
#define SACN_RECEIVER_MAX_SOCKET_REFS \
  ((((SACN_RECEIVER_MAX_UNIVERSES - 1) / SACN_RECEIVER_MAX_SUBS_PER_SOCKET) + 1) * 2)

typedef unsigned int sacn_thread_id_t;
#define SACN_THREAD_ID_INVALID UINT_MAX

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

#define UNIVERSE_ID_VALID(universe_id) ((universe_id != 0) && (universe_id <= 64000))

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
  /* (optional) array of network interfaces on which to listen to the specified universe. If num_netints = 0,
   * all available network interfaces will be used. */
#if SACN_DYNAMIC_MEM
  EtcPalMcastNetintId* netints;
#else
  EtcPalMcastNetintId netints[SACN_MAX_NETINTS];
#endif
  /* Number of elements in the netints array. */
  size_t num_netints;

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

typedef struct SourceDataBuffer
{
  /* Whether this buffer has been written to yet. */
  bool written;
  /* The address from which we received this data. */
  EtcPalSockAddr from_addr;
  /* The priority of the sACN data. Valid range is 0-200, inclusive. */
  uint8_t priority;
  /* Whether the Preview_Data bit is set for the sACN data. From E1.31: "Indicates that the data in
   * this packet is intended for use in visualization or media server preview applications and
   * shall not be used to generate live output." */
  bool preview;
  /* The start code of the DMX data. */
  uint8_t start_code;
  /* The number of slots in the DMX data. */
  uint16_t slot_count;
  /* The DMX data. */
  uint8_t data[DMX_ADDRESS_COUNT];
} SourceDataBuffer;

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

/******************************************************************************
 * Notifications delivered by the sACN receive module
 *****************************************************************************/

/* Data for the universe_data() callback */
typedef struct UniverseDataNotification
{
  SacnUniverseDataCallback callback;
  sacn_receiver_t handle;
  uint16_t universe;
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
  etcpal_socket_t sock;     /* The socket descriptor. */
  size_t refcount;          /* How many addresses the socket is subscribed to. */
  etcpal_iptype_t ip_type;  /* The IP type used in multicast subscriptions and the bind address. */
#if SACN_RECEIVER_LIMIT_BIND
  bool bound;               /* True if bind was called on this socket, false otherwise. */
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
