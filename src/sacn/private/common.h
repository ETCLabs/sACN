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
#include "etcpal/mutex.h"
#include "etcpal/log.h"
#include "etcpal/rbtree.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "sacn/receiver.h"
#include "sacn/merge_receiver.h"
#include "sacn/source.h"
#include "sacn/source_detector.h"
#include "sacn/dmx_merger.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Global constants, macros, types, etc.
 *****************************************************************************/

#define SACN_DATA_PACKET_MTU 638
#define SACN_UNIVERSE_DISCOVERY_PACKET_MTU 1144
#define SACN_MTU SACN_UNIVERSE_DISCOVERY_PACKET_MTU
#define SACN_PORT 5568

#define SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE 512
#define SACN_DISCOVERY_UNIVERSE 64214
#define SACN_UNIVERSE_DISCOVERY_INTERVAL 10000

#define SACN_STATS_LOG_INTERVAL 10000

/* The source-loss timeout, defined in E1.31 as network data loss */
#define SACN_SOURCE_LOSS_TIMEOUT 2500
/* How long to wait for a 0xdd packet once a new source is discovered */
#define SACN_WAIT_FOR_PRIORITY 1500
/* Length of the sampling period for a new universe */
#define SACN_SAMPLE_TIME 1500

/*
 * This ensures there are always enough SocketRefs. This is multiplied by 2 because SocketRefs come in pairs - one for
 * IPv4, and another for IPv6. This is because a single SocketRef cannot intermix IPv4 and IPv6.
 *
 * If SACN_RECEIVER_SOCKET_PER_NIC is enabled, this is further multiplied by the max number of NICs.
 */
#if SACN_RECEIVER_SOCKET_PER_NIC
#define SACN_RECEIVER_MAX_SOCKET_REFS \
  ((((SACN_RECEIVER_MAX_UNIVERSES - 1) / SACN_RECEIVER_MAX_SUBS_PER_SOCKET) + 1) * 2 * SACN_MAX_NETINTS)
#else
#define SACN_RECEIVER_MAX_SOCKET_REFS \
  ((((SACN_RECEIVER_MAX_UNIVERSES - 1) / SACN_RECEIVER_MAX_SUBS_PER_SOCKET) + 1) * 2)
#endif

typedef unsigned int sacn_thread_id_t;
#define SACN_THREAD_ID_INVALID UINT_MAX

#define UNIVERSE_ID_VALID(universe_id) ((universe_id != 0) && (universe_id < 64000))

#define SACN_RECEIVER_ENABLED                                                                                 \
  ((!SACN_DYNAMIC_MEM && (SACN_RECEIVER_MAX_UNIVERSES > 0) && (SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE > 0) && \
    (SACN_RECEIVER_TOTAL_MAX_SOURCES > 0)) ||                                                                 \
   SACN_DYNAMIC_MEM)

#define SACN_SOURCE_ENABLED                                                                              \
  ((!SACN_DYNAMIC_MEM && (SACN_SOURCE_MAX_SOURCES > 0) && (SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE > 0)) || \
   SACN_DYNAMIC_MEM)
#define SACN_SOURCE_UNICAST_ENABLED \
  ((!SACN_DYNAMIC_MEM && (SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE > 0)) || SACN_DYNAMIC_MEM)

#define SACN_DMX_MERGER_ENABLED                                                                                \
  ((!SACN_DYNAMIC_MEM && (SACN_DMX_MERGER_MAX_MERGERS > 0) && (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER > 0)) || \
   SACN_DYNAMIC_MEM)

#define SACN_SOURCE_DETECTOR_ENABLED                                                        \
  ((!SACN_DYNAMIC_MEM && SACN_RECEIVER_ENABLED && (SACN_SOURCE_DETECTOR_MAX_SOURCES > 0) && \
    (SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE > 0)) ||                                 \
   SACN_DYNAMIC_MEM)

#define SACN_MERGE_RECEIVER_ENABLED (SACN_MERGE_RECEIVER_ENABLE_IN_STATIC_MEMORY_MODE || SACN_DYNAMIC_MEM)

#if SACN_SOURCE_DETECTOR_ENABLED
#define SACN_MAX_SUBSCRIPTIONS ((SACN_RECEIVER_MAX_UNIVERSES + 1) * 2)
#elif SACN_RECEIVER_ENABLED
#define SACN_MAX_SUBSCRIPTIONS (SACN_RECEIVER_MAX_UNIVERSES * 2)
#else
#define SACN_MAX_SUBSCRIPTIONS (0)
#endif

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
 *
 * There are also variations of SACN_DECLARE_BUF for each API below, factoring in if the memory is enabled or not.
 */
#if SACN_DYNAMIC_MEM
#define SACN_DECLARE_BUF(type, name, max_static_size) \
  type* name;                                         \
  size_t name##_capacity
#else
#define SACN_DECLARE_BUF(type, name, max_static_size) type name[max_static_size]
#endif

#if SACN_RECEIVER_ENABLED
#define SACN_DECLARE_RECEIVER_BUF(type, name, size) SACN_DECLARE_BUF(type, name, size)
#else
#define SACN_DECLARE_RECEIVER_BUF(type, name, size) type* name
#endif

#if SACN_MERGE_RECEIVER_ENABLED
#define SACN_DECLARE_MERGE_RECEIVER_BUF(type, name, size) SACN_DECLARE_BUF(type, name, size)
#else
#define SACN_DECLARE_MERGE_RECEIVER_BUF(type, name, size) type* name
#endif

#if SACN_SOURCE_ENABLED
#define SACN_DECLARE_SOURCE_BUF(type, name, size) SACN_DECLARE_BUF(type, name, size)
#else
#define SACN_DECLARE_SOURCE_BUF(type, name, size) type* name
#endif

#if SACN_SOURCE_DETECTOR_ENABLED
#define SACN_DECLARE_SOURCE_DETECTOR_BUF(type, name, size) SACN_DECLARE_BUF(type, name, size)
#else
#define SACN_DECLARE_SOURCE_DETECTOR_BUF(type, name, size) type* name
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

typedef struct SacnInternalSocketState
{
#if SACN_RECEIVER_SOCKET_PER_NIC
  SACN_DECLARE_BUF(etcpal_socket_t, ipv4_sockets, SACN_MAX_NETINTS);
  SACN_DECLARE_BUF(etcpal_socket_t, ipv6_sockets, SACN_MAX_NETINTS);
  size_t num_ipv4_sockets;
  size_t num_ipv6_sockets;
#else
  etcpal_socket_t ipv4_socket;
  etcpal_socket_t ipv6_socket;
#endif
} SacnInternalSocketState;

/******************************************************************************
 * Types used by the source loss module
 *****************************************************************************/

typedef struct SacnRemoteSourceInternal
{
  sacn_remote_source_t handle;
  const char* name;
} SacnRemoteSourceInternal;

typedef struct SacnLostSourceInternal
{
  sacn_remote_source_t handle;
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

/* A key to uniquely identify a source in a termination set. */
typedef struct TerminationSetSourceKey
{
  sacn_remote_source_t handle;
  uint16_t universe;
} TerminationSetSourceKey;

/* A source in a termination set. Sources are removed from the termination set as they are
 * determined to be online. */
typedef struct TerminationSetSource
{
  TerminationSetSourceKey key;  // Must remain the first element in the struct for red-black tree lookup.
  const char* name;
  bool offline;
  bool terminated;
} TerminationSetSource;

/******************************************************************************
 * Types used by the sACN Source Detector module
 *****************************************************************************/

typedef struct SacnSourceDetector
{
  // Identification
  sacn_thread_id_t thread_id;

  // Sockets / network interface info
  SacnInternalSocketState sockets;
  /* Array of network interfaces on which to listen to the specified universe. */
  SacnInternalNetintArray netints;

  // State tracking
  bool created;
  bool suppress_source_limit_exceeded_notification;

  // Configured callbacks
  SacnSourceDetectorCallbacks callbacks;

  /* The maximum number of sources the detector will record.  It is recommended that applications using dynamic
   * memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when configured to use
   * static memory -- #SACN_SOURCE_DETECTOR_MAX_SOURCES is used instead.*/
  int source_count_max;

  /* The maximum number of universes the detector will record for a source.  It is recommended that applications using
   * dynamic memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when configured to
   * use static memory -- #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE is used instead.*/
  int universes_per_source_max;

  /* What IP networking the source detector will support.  The default is #kSacnIpV4AndIpV6. */
  sacn_ip_support_t ip_supported;
} SacnSourceDetector;

typedef struct SacnUniverseDiscoverySource
{
  sacn_remote_source_t handle;  // This must be the first member.
  char name[SACN_SOURCE_NAME_MAX_LEN];

  SACN_DECLARE_SOURCE_DETECTOR_BUF(uint16_t, universes, SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE);
  size_t num_universes;
  bool universes_dirty;  // The universe list has un-notified changes.
  size_t last_notified_universe_count;
  bool suppress_universe_limit_exceeded_notification;

  EtcPalTimer expiration_timer;
  size_t next_universe_index;
  int next_page;
} SacnUniverseDiscoverySource;

typedef struct SacnUniverseDiscoveryPage
{
  const EtcPalUuid* sender_cid;
  const EtcPalSockAddr* from_addr;
  const char* source_name;
  int page;
  int last_page;
  const uint16_t* universes;
  size_t num_universes;
} SacnUniverseDiscoveryPage;

/******************************************************************************
 * Notifications delivered by the sACN Source Detector module
 *****************************************************************************/

typedef struct SourceDetectorSourceUpdatedNotification
{
  SacnSourceDetectorSourceUpdatedCallback callback;
  sacn_remote_source_t handle;
  const EtcPalUuid* cid;
  const char* name;
#if SACN_DYNAMIC_MEM
  uint16_t* sourced_universes;
#elif SACN_SOURCE_DETECTOR_ENABLED
  uint16_t sourced_universes[SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE];
#else
  uint16_t* sourced_universes;  // This is only here so things compile.
#endif
  size_t num_sourced_universes;
  void* context;
} SourceDetectorSourceUpdatedNotification;

#if SACN_DYNAMIC_MEM
#define SRC_DETECTOR_SOURCE_UPDATED_DEFAULT_INIT                \
  {                                                             \
    NULL, SACN_REMOTE_SOURCE_INVALID, NULL, NULL, NULL, 0, NULL \
  }
#elif SACN_SOURCE_DETECTOR_ENABLED
#define SRC_DETECTOR_SOURCE_UPDATED_DEFAULT_INIT               \
  {                                                            \
    NULL, SACN_REMOTE_SOURCE_INVALID, NULL, NULL, {0}, 0, NULL \
  }
#else
#define SRC_DETECTOR_SOURCE_UPDATED_DEFAULT_INIT                \
  {                                                             \
    NULL, SACN_REMOTE_SOURCE_INVALID, NULL, NULL, NULL, 0, NULL \
  }
#endif

typedef struct SourceDetectorExpiredSource
{
  sacn_remote_source_t handle;
  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];
} SourceDetectorExpiredSource;

#define SRC_DETECTOR_EXPIRED_SOURCE_DEFAULT_INIT \
  {                                              \
    SACN_REMOTE_SOURCE_INVALID, {{0}}, { 0 }     \
  }

typedef struct SourceDetectorSourceExpiredNotification
{
  SacnSourceDetectorSourceExpiredCallback callback;
  SACN_DECLARE_SOURCE_DETECTOR_BUF(SourceDetectorExpiredSource, expired_sources, SACN_SOURCE_DETECTOR_MAX_SOURCES);
  size_t num_expired_sources;
  void* context;
} SourceDetectorSourceExpiredNotification;

#if SACN_DYNAMIC_MEM
#define SRC_DETECTOR_SOURCE_EXPIRED_DEFAULT_INIT \
  {                                              \
    NULL, NULL, 0, 0, NULL                       \
  }
#elif SACN_SOURCE_DETECTOR_ENABLED
#define SRC_DETECTOR_SOURCE_EXPIRED_DEFAULT_INIT              \
  {                                                           \
    NULL, {SRC_DETECTOR_EXPIRED_SOURCE_DEFAULT_INIT}, 0, NULL \
  }
#else
#define SRC_DETECTOR_SOURCE_EXPIRED_DEFAULT_INIT \
  {                                              \
    NULL, NULL, 0, NULL                          \
  }
#endif

typedef struct SourceDetectorLimitExceededNotification
{
  SacnSourceDetectorLimitExceededCallback callback;
  void* context;
} SourceDetectorLimitExceededNotification;

#define SRC_DETECTOR_LIMIT_EXCEEDED_DEFAULT_INIT \
  {                                              \
    NULL, NULL                                   \
  }

/******************************************************************************
 * Types used by the sACN Receive module
 *****************************************************************************/

typedef void (*SacnUniverseDataInternalCallback)(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                                 const SacnRemoteSource* source_info,
                                                 const SacnRecvUniverseData* universe_data, sacn_thread_id_t thread_id);
typedef void (*SacnSourcesLostInternalCallback)(sacn_receiver_t handle, uint16_t universe,
                                                const SacnLostSource* lost_sources, size_t num_lost_sources,
                                                sacn_thread_id_t thread_id);
typedef void (*SacnSamplingPeriodStartedInternalCallback)(sacn_receiver_t handle, uint16_t universe,
                                                          sacn_thread_id_t thread_id);
typedef void (*SacnSamplingPeriodEndedInternalCallback)(sacn_receiver_t handle, uint16_t universe,
                                                        sacn_thread_id_t thread_id);
typedef void (*SacnSourcePapLostInternalCallback)(sacn_receiver_t handle, uint16_t universe,
                                                  const SacnRemoteSource* source, sacn_thread_id_t thread_id);
typedef void (*SacnSourceLimitExceededInternalCallback)(sacn_receiver_t handle, uint16_t universe,
                                                        sacn_thread_id_t thread_id);

/* Custom versions of receiver callbacks that include the thread ID (for the merge receiver). */
typedef struct SacnReceiverInternalCallbacks
{
  SacnUniverseDataInternalCallback universe_data;
  SacnSourcesLostInternalCallback sources_lost;
  SacnSamplingPeriodStartedInternalCallback sampling_period_started;
  SacnSamplingPeriodEndedInternalCallback sampling_period_ended;
  SacnSourcePapLostInternalCallback source_pap_lost;
  SacnSourceLimitExceededInternalCallback source_limit_exceeded;
} SacnReceiverInternalCallbacks;

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
  SacnReceiverKeys keys;  // This must be the first member.
  sacn_thread_id_t thread_id;

  // Sockets / network interface info
  SacnInternalSocketState sockets;
  /* Array of network interfaces on which to listen to the specified universe. */
  SacnInternalNetintArray netints;

  // State tracking
  bool sampling;
  bool notified_sampling_started;
  EtcPalTimer sample_timer;
  EtcPalRbTree sampling_period_netints;

  bool suppress_limit_exceeded_notification;
  EtcPalRbTree sources;       // The sources being tracked on this universe.
  TerminationSet* term_sets;  // Source loss tracking

  // Option flags
  bool filter_preview_data;

  // Configured callbacks
  SacnReceiverCallbacks api_callbacks;
  SacnReceiverInternalCallbacks internal_callbacks;

  /* The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
   * When configured to use static memory, this parameter is only used if it's less than
   * #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE -- otherwise #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used instead. */
  size_t source_count_max;

  /* What IP networking the receiver will support. */
  sacn_ip_support_t ip_supported;

  SacnReceiver* next;
};

/* A set of linked lists to track the state of sources in the tick function. */
typedef struct SacnSourceStatusLists
{
  SACN_DECLARE_RECEIVER_BUF(SacnLostSourceInternal, offline, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  SACN_DECLARE_RECEIVER_BUF(SacnRemoteSourceInternal, online, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  SACN_DECLARE_RECEIVER_BUF(SacnRemoteSourceInternal, unknown, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_offline;
  size_t num_online;
  size_t num_unknown;
} SacnSourceStatusLists;

#if SACN_ETC_PRIORITY_EXTENSION
typedef enum
{
  kRecvStateWaitingForPap,
  kRecvStateHaveDmxOnly,
  kRecvStateHavePapOnly,
  kRecvStateHaveDmxAndPap
} sacn_recv_state_t;
#endif

/* An sACN source that is being tracked on a given universe. */
typedef struct SacnTrackedSource
{
  sacn_remote_source_t handle;  // This must be the first member of this struct.
  char name[SACN_SOURCE_NAME_MAX_LEN];
  EtcPalMcastNetintId netint;

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

typedef struct SacnRemoteSourceHandle
{
  EtcPalUuid cid;  // This must be the first member of this struct.
  sacn_remote_source_t handle;
} SacnRemoteSourceHandle;

typedef struct SacnRemoteSourceCid
{
  sacn_remote_source_t handle;  // This must be the first member of this struct.
  EtcPalUuid cid;
  size_t refcount;
} SacnRemoteSourceCid;

typedef enum
{
  kPerformAllSocketCleanupNow,
  kQueueSocketCleanup
} socket_cleanup_behavior_t;

typedef struct SacnSamplingPeriodNetint
{
  EtcPalMcastNetintId id;          // This must be the first member of this struct.
  bool in_future_sampling_period;  // True if this is in the NEXT sampling period, false if in the current one.
} SacnSamplingPeriodNetint;

/******************************************************************************
 * Notifications delivered by the sACN receive module
 *****************************************************************************/

/* Data for the universe_data() callback */
typedef struct UniverseDataNotification
{
  SacnUniverseDataCallback api_callback;
  SacnUniverseDataInternalCallback internal_callback;
  sacn_receiver_t receiver_handle;
  SacnRemoteSource source_info;
  SacnRecvUniverseData universe_data;
  sacn_thread_id_t thread_id;
  void* context;
} UniverseDataNotification;

/* Data for the sources_lost() callback */
typedef struct SourcesLostNotification
{
  SacnSourcesLostCallback api_callback;
  SacnSourcesLostInternalCallback internal_callback;
  sacn_receiver_t handle;
  uint16_t universe;
  SACN_DECLARE_RECEIVER_BUF(SacnLostSource, lost_sources, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_lost_sources;
  sacn_thread_id_t thread_id;
  void* context;
} SourcesLostNotification;

/* Data for the sampling_period_started() callback */
typedef struct SamplingStartedNotification
{
  SacnSamplingPeriodStartedCallback api_callback;
  SacnSamplingPeriodStartedInternalCallback internal_callback;
  sacn_receiver_t handle;
  uint16_t universe;
  sacn_thread_id_t thread_id;
  void* context;
} SamplingStartedNotification;

/* Data for the sampling_period_ended() callback */
typedef struct SamplingEndedNotification
{
  SacnSamplingPeriodEndedCallback api_callback;
  SacnSamplingPeriodEndedInternalCallback internal_callback;
  sacn_receiver_t handle;
  uint16_t universe;
  sacn_thread_id_t thread_id;
  void* context;
} SamplingEndedNotification;

/* Data for the source_pap_lost() callback */
typedef struct SourcePapLostNotification
{
  SacnSourcePapLostCallback api_callback;
  SacnSourcePapLostInternalCallback internal_callback;
  SacnRemoteSource source;
  sacn_receiver_t handle;
  uint16_t universe;
  sacn_thread_id_t thread_id;
  void* context;
} SourcePapLostNotification;

/* Data for the source_limit_exceeded() callback */
typedef struct SourceLimitExceededNotification
{
  SacnSourceLimitExceededCallback api_callback;
  SacnSourceLimitExceededInternalCallback internal_callback;
  sacn_receiver_t handle;
  uint16_t universe;
  sacn_thread_id_t thread_id;
  void* context;
} SourceLimitExceededNotification;

/* Contains commonly used information about a sACN socket used for receiving. */
typedef struct ReceiveSocket
{
  etcpal_socket_t handle;  /* The socket descriptor. */
  etcpal_iptype_t ip_type; /* The IP type used in multicast subscriptions and the bind address. */
  bool bound;              /* True if bind was called on this socket, false otherwise. */
  bool polling;            /* True if this socket was added to a poll context, false otherwise. */
#if SACN_RECEIVER_SOCKET_PER_NIC
  unsigned int ifindex; /* Index of network interface on which this socket is subscribed. */
#endif
} ReceiveSocket;

#define RECV_SOCKET_DEFAULT_INIT                              \
  {                                                           \
    ETCPAL_SOCKET_INVALID, kEtcPalIpTypeInvalid, false, false \
  }

/* For the shared-socket model, this represents a shared socket. */
typedef struct SocketRef
{
  ReceiveSocket socket; /* The socket handle, IP type, and state. */
  size_t refcount;      /* How many addresses the socket is subscribed to. */
  bool pending;         /* Whether or not this SocketRef is pending queued operations on the thread. */
} SocketRef;

/* Queued information for joining and leaving multicast groups. */
typedef struct SocketGroupReq
{
  etcpal_socket_t socket; /* The socket descriptor. */
  EtcPalGroupReq group;   /* The interface and group address to join or leave. */
} SocketGroupReq;

/* Holds the discrete data used by each receiver thread. */
typedef struct SacnRecvThreadContext
{
  sacn_thread_id_t thread_id;
  etcpal_thread_t thread_handle;
  bool running;

  SacnReceiver* receivers;
  size_t num_receivers;

  // Only one thread will ever have a source detector, because the library can only create one source detector instance.
  SacnSourceDetector* source_detector;

  // We do most interactions with sockets from the same thread that we receive from them, to avoid
  // thread safety foibles on some platforms. So, sockets to add and remove from the thread's
  // polling context are queued to be acted on from the thread.
  SACN_DECLARE_RECEIVER_BUF(ReceiveSocket, dead_sockets, SACN_RECEIVER_MAX_UNIVERSES * 2);
  size_t num_dead_sockets;

  SACN_DECLARE_BUF(SocketRef, socket_refs, SACN_RECEIVER_MAX_SOCKET_REFS);
  size_t num_socket_refs;
  size_t new_socket_refs;

  // Socket subscription operations also get queued to be acted on from the thread.
  SACN_DECLARE_RECEIVER_BUF(SocketGroupReq, subscribes, (SACN_MAX_NETINTS * SACN_MAX_SUBSCRIPTIONS));
  SACN_DECLARE_RECEIVER_BUF(SocketGroupReq, unsubscribes, (SACN_MAX_NETINTS * SACN_MAX_SUBSCRIPTIONS));
  size_t num_subscribes;
  size_t num_unsubscribes;

#if SACN_RECEIVER_LIMIT_BIND
  bool ipv4_bound;
  bool ipv6_bound;
#endif

  // This section is only touched from the thread, outside the lock.
  EtcPalPollContext poll_context;
  bool poll_context_initialized;
  uint8_t recv_buf[SACN_MTU];
  EtcPalTimer periodic_timer;
  bool periodic_timer_started;
} SacnRecvThreadContext;

/******************************************************************************
 * Types used by the sACN Merge Receiver module
 *****************************************************************************/
typedef struct SacnMergeReceiverInternalSource
{
  sacn_remote_source_t handle;  // This must be the first struct member.
  char name[SACN_SOURCE_NAME_MAX_LEN];
  EtcPalSockAddr addr;
  bool sampling;
} SacnMergeReceiverInternalSource;

typedef struct SacnMergeReceiver
{
  sacn_merge_receiver_t merge_receiver_handle;  // This must be the first struct member.
  SacnMergeReceiverCallbacks callbacks;
  bool use_pap;

  sacn_dmx_merger_t merger_handle;
  uint8_t levels[DMX_ADDRESS_COUNT];
  uint8_t priorities[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_source_t owners[DMX_ADDRESS_COUNT];

#if SACN_MERGE_RECEIVER_ENABLE_SAMPLING_MERGER
  sacn_dmx_merger_t sampling_merger_handle;
  uint8_t sampling_levels[DMX_ADDRESS_COUNT];
  uint8_t sampling_priorities[DMX_ADDRESS_COUNT];
  sacn_dmx_merger_source_t sampling_owners[DMX_ADDRESS_COUNT];
#endif

  EtcPalRbTree sources;

  bool sampling;
} SacnMergeReceiver;

/******************************************************************************
 * Notifications delivered by the sACN Merge Receiver module
 *****************************************************************************/

typedef struct MergeReceiverMergedDataNotification
{
  SacnMergeReceiverMergedDataCallback callback;
  sacn_merge_receiver_t handle;
  uint16_t universe;
  SacnRecvUniverseSubrange slot_range;
  uint8_t levels[DMX_ADDRESS_COUNT];
  uint8_t priorities[DMX_ADDRESS_COUNT];
  sacn_remote_source_t owners[DMX_ADDRESS_COUNT];
  SACN_DECLARE_MERGE_RECEIVER_BUF(sacn_remote_source_t, active_sources, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
  size_t num_active_sources;
} MergeReceiverMergedDataNotification;

/******************************************************************************
 * Types used by the sACN Source module
 *****************************************************************************/

typedef enum
{
  kTerminatingAndRemoving,
  kTerminatingWithoutRemoving,
  kNotTerminating
} termination_state_t;

typedef struct SacnSourceNetint
{
  EtcPalMcastNetintId id;  // This must be the first struct member.
  size_t num_refs;         // Number of universes using this netint.
} SacnSourceNetint;

typedef struct SacnUnicastDestination
{
  EtcPalIpAddr dest_addr;  // This must be the first struct member.
  termination_state_t termination_state;
  int num_terminations_sent;
  etcpal_error_t last_send_error;
} SacnUnicastDestination;

typedef struct SacnSourceUniverse
{
  uint16_t universe_id;  // This must be the first struct member.

  termination_state_t termination_state;
  int num_terminations_sent;

  uint8_t priority;
  uint16_t sync_universe;
  bool send_preview;
  uint8_t next_seq_num;

  // Start code 0x00 state
  int level_packets_sent_before_suppression;
  EtcPalTimer level_keep_alive_timer;
  uint8_t level_send_buf[SACN_DATA_PACKET_MTU];
  bool has_level_data;
  bool levels_sent_this_tick;

#if SACN_ETC_PRIORITY_EXTENSION
  // Start code 0xDD state
  int pap_packets_sent_before_suppression;
  EtcPalTimer pap_keep_alive_timer;
  uint8_t pap_send_buf[SACN_DATA_PACKET_MTU];
  bool has_pap_data;
  bool pap_sent_this_tick;
#endif

  bool other_sent_this_tick;
  bool anything_sent_this_tick;

  SACN_DECLARE_BUF(SacnUnicastDestination, unicast_dests, SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE);
  size_t num_unicast_dests;
  bool send_unicast_only;

  etcpal_error_t last_send_error;

  SacnInternalNetintArray netints;
} SacnSourceUniverse;

typedef struct SacnSource
{
  sacn_source_t handle;  // This must be the first struct member.

  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];

  bool terminating;  // If in the process of terminating all universes and removing this source.

  SACN_DECLARE_SOURCE_BUF(SacnSourceUniverse, universes, SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE);
  size_t num_universes;
  size_t num_active_universes;  // Number of universes to include in universe discovery packets.
  EtcPalTimer universe_discovery_timer;
  bool process_manually;
  sacn_ip_support_t ip_supported;
  int keep_alive_interval;
  int pap_keep_alive_interval;
  size_t universe_count_max;

  EtcPalTimer stats_log_timer;  // Maintains a repeating interval, at the end of which statistics are logged
  int total_tick_count;         // The total number of ticks this interval
  int failed_tick_count;        // The number of ticks this interval that failed at least one send

  // This is the set of unique netints used by all universes of this source, to be used when transmitting universe
  // discovery packets.
  SACN_DECLARE_BUF(SacnSourceNetint, netints, SACN_MAX_NETINTS);
  size_t num_netints;

  uint8_t universe_discovery_send_buf[SACN_UNIVERSE_DISCOVERY_PACKET_MTU];
} SacnSource;

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
