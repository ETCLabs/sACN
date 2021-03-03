/******************************************************************************
 * Copyright 2021 ETC Inc.
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

#include "sacn/private/mem.h"

#include <stddef.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/**************************** Private constants ******************************/

#define INITIAL_CAPACITY 8

#define SACN_RECEIVER_MAX_RB_NODES ((SACN_RECEIVER_MAX_UNIVERSES * 2) + SACN_RECEIVER_TOTAL_MAX_SOURCES)

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

#define CHECK_CAPACITY(container, size_requested, buffer, buffer_type, max_static, failure_return_value)        \
  do                                                                                                            \
  {                                                                                                             \
    if (size_requested > container->buffer##_capacity)                                                          \
    {                                                                                                           \
      size_t new_capacity = grow_capacity(container->buffer##_capacity, size_requested);                        \
      buffer_type* new_##buffer = (buffer_type*)realloc(container->buffer, new_capacity * sizeof(buffer_type)); \
      if (new_##buffer)                                                                                         \
      {                                                                                                         \
        container->buffer = new_##buffer;                                                                       \
        container->buffer##_capacity = new_capacity;                                                            \
      }                                                                                                         \
      else                                                                                                      \
      {                                                                                                         \
        return failure_return_value;                                                                            \
      }                                                                                                         \
    }                                                                                                           \
  } while (0)

#define CHECK_ROOM_FOR_ONE_MORE(container, buffer, buffer_type, max_static, failure_return_value) \
  CHECK_CAPACITY(container, container->num_##buffer + 1, buffer, buffer_type, max_static, failure_return_value)

/* Macros for dynamic allocation. */
#define ALLOC_RECEIVER() malloc(sizeof(SacnReceiver))
#define ALLOC_TRACKED_SOURCE() malloc(sizeof(SacnTrackedSource))
#define FREE_RECEIVER(ptr)        \
  do                              \
  {                               \
    if (ptr->netints.netints)     \
    {                             \
      free(ptr->netints.netints); \
    }                             \
    free(ptr);                    \
  } while (0)
#define FREE_TRACKED_SOURCE(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

#define CHECK_CAPACITY(container, size_requested, buffer, buffer_type, max_static, failure_return_value) \
  do                                                                                                     \
  {                                                                                                      \
    if (size_requested > max_static)                                                                     \
    {                                                                                                    \
      return failure_return_value;                                                                       \
    }                                                                                                    \
  } while (0)

#define CHECK_ROOM_FOR_ONE_MORE(container, buffer, buffer_type, max_static, failure_return_value) \
  CHECK_CAPACITY(container, container->num_##buffer + 1, buffer, buffer_type, max_static, failure_return_value)

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_RECEIVER() etcpal_mempool_alloc(sacnrecv_receivers)
#define ALLOC_TRACKED_SOURCE() etcpal_mempool_alloc(sacnrecv_tracked_sources)
#define FREE_RECEIVER(ptr) etcpal_mempool_free(sacnrecv_receivers, ptr)
#define FREE_TRACKED_SOURCE(ptr) etcpal_mempool_free(sacnrecv_tracked_sources, ptr)

#endif  // SACN_DYNAMIC_MEM

#define REMOVE_AT_INDEX(container, buffer_type, buffer, index)          \
  do                                                                    \
  {                                                                     \
    --container->num_##buffer;                                          \
                                                                        \
    if (index < container->num_##buffer)                                \
    {                                                                   \
      memmove(&container->buffer[index], &container->buffer[index + 1], \
              (container->num_##buffer - index) * sizeof(buffer_type)); \
    }                                                                   \
  } while (0)

/****************************** Private types ********************************/

typedef struct SourcesLostNotificationBuf
{
  SACN_DECLARE_BUF(SourcesLostNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SourcesLostNotificationBuf;

typedef struct SamplingStartedNotificationBuf
{
  SACN_DECLARE_BUF(SamplingStartedNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SamplingStartedNotificationBuf;

typedef struct SamplingEndedNotificationBuf
{
  SACN_DECLARE_BUF(SamplingEndedNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SamplingEndedNotificationBuf;

typedef struct ToEraseBuf
{
  SACN_DECLARE_BUF(SacnTrackedSource*, buf, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
} ToEraseBuf;

/**************************** Private variables ******************************/

#if SACN_SOURCE_ENABLED
static bool sources_initialized = false;
#endif

static struct SacnMemBufs
{
  unsigned int num_threads;

#if SACN_DYNAMIC_MEM
  SacnSourceStatusLists* status_lists;
  ToEraseBuf* to_erase;
  SacnRecvThreadContext* recv_thread_context;

  UniverseDataNotification* universe_data;
  SourcesLostNotificationBuf* sources_lost;
  SourcePapLostNotification* source_pap_lost;
  SamplingStartedNotificationBuf* sampling_started;
  SamplingEndedNotificationBuf* sampling_ended;
  SourceLimitExceededNotification* source_limit_exceeded;
#else   // SACN_DYNAMIC_MEM
  SacnSourceStatusLists status_lists[SACN_RECEIVER_MAX_THREADS];
  ToEraseBuf to_erase[SACN_RECEIVER_MAX_THREADS];
  SacnRecvThreadContext recv_thread_context[SACN_RECEIVER_MAX_THREADS];

  UniverseDataNotification universe_data[SACN_RECEIVER_MAX_THREADS];
  SourcesLostNotificationBuf sources_lost[SACN_RECEIVER_MAX_THREADS];
  SourcePapLostNotification source_pap_lost[SACN_RECEIVER_MAX_THREADS];
  SamplingStartedNotificationBuf sampling_started[SACN_RECEIVER_MAX_THREADS];
  SamplingEndedNotificationBuf sampling_ended[SACN_RECEIVER_MAX_THREADS];
  SourceLimitExceededNotification source_limit_exceeded[SACN_RECEIVER_MAX_THREADS];
#endif  // SACN_DYNAMIC_MEM

#if SACN_SOURCE_ENABLED
  SACN_DECLARE_BUF(SacnSource, sources, SACN_SOURCE_MAX_SOURCES);
  size_t num_sources;
#endif
} mem_bufs;

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacnrecv_receivers, SacnReceiver, SACN_RECEIVER_MAX_UNIVERSES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_tracked_sources, SacnTrackedSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_rb_nodes, EtcPalRbNode, SACN_RECEIVER_MAX_RB_NODES);
#endif

static EtcPalRbTree receivers;
static EtcPalRbTree receivers_by_universe;

/*********************** Private function prototypes *************************/

static void zero_status_lists(SacnSourceStatusLists* status_lists);
static void zero_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size);

static size_t get_source_index(sacn_source_t handle, bool* found);
static size_t get_source_universe_index(SacnSource* source, uint16_t universe, bool* found);
static size_t get_unicast_dest_index(SacnSourceUniverse* universe, const EtcPalIpAddr* addr, bool* found);
static size_t get_source_netint_index(SacnSource* source, const EtcPalMcastNetintId* id, bool* found);

#if SACN_DYNAMIC_MEM
static size_t grow_capacity(size_t old_capacity, size_t capacity_requested);
#endif

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_status_lists_buf(unsigned int num_threads);
static etcpal_error_t init_status_lists_entry(SacnSourceStatusLists* status_lists);

static etcpal_error_t init_to_erase_bufs(unsigned int num_threads);
static etcpal_error_t init_to_erase_buf(ToEraseBuf* to_erase_buf);

static etcpal_error_t init_recv_thread_context_buf(unsigned int num_threads);
static etcpal_error_t init_recv_thread_context_entry(SacnRecvThreadContext* recv_thread_context);

static etcpal_error_t init_universe_data_buf(unsigned int num_threads);

static etcpal_error_t init_sources_lost_bufs(unsigned int num_threads);
static etcpal_error_t init_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf);
static etcpal_error_t init_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size);

static etcpal_error_t init_source_pap_lost_buf(unsigned int num_threads);

static etcpal_error_t init_sampling_started_bufs(unsigned int num_threads);
static etcpal_error_t init_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf);
static etcpal_error_t init_sampling_ended_bufs(unsigned int num_threads);
static etcpal_error_t init_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf);

static etcpal_error_t init_source_limit_exceeded_buf(unsigned int num_threads);

// Dynamic memory deinitialization
static void deinit_status_lists_buf(void);
static void deinit_status_lists_entry(SacnSourceStatusLists* status_lists);

static void deinit_to_erase_bufs(void);
static void deinit_to_erase_buf(ToEraseBuf* to_erase_buf);

static void deinit_recv_thread_context_buf(void);
static void deinit_recv_thread_context_entry(SacnRecvThreadContext* recv_thread_context);

static void deinit_universe_data_buf(void);

static void deinit_sources_lost_bufs(void);
static void deinit_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf);
static void deinit_sources_lost_entry(SourcesLostNotification* sources_lost);

static void deinit_source_pap_lost_buf(void);

static void deinit_sampling_started_bufs(void);
static void deinit_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf);
static void deinit_sampling_ended_bufs(void);
static void deinit_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf);

static void deinit_source_limit_exceeded_buf(void);
#endif  // SACN_DYNAMIC_MEM

// Receiver memory management
static etcpal_error_t insert_receiver_into_maps(SacnReceiver* receiver);
static void remove_receiver_from_maps(SacnReceiver* receiver);

// Receiver tree node management
static int tracked_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare_by_universe(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* node_alloc(void);
static void node_dealloc(EtcPalRbNode* node);
static void source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

// Sources initialization/deinitialization
static etcpal_error_t init_sources(void);
static void deinit_sources(void);

// Receivers initialization/deinitialization
static etcpal_error_t init_receivers(void);
static void deinit_receivers(void);

/*************************** Function definitions ****************************/

/*
 * Initialize the memory module.
 */
etcpal_error_t sacn_mem_init(unsigned int num_threads)
{
#if !SACN_DYNAMIC_MEM
  if (num_threads > SACN_RECEIVER_MAX_THREADS)
    return kEtcPalErrNoMem;
#endif
  mem_bufs.num_threads = num_threads;

  etcpal_error_t res = kEtcPalErrOk;

#if SACN_DYNAMIC_MEM
  res = init_status_lists_buf(num_threads);
  if (res == kEtcPalErrOk)
    res = init_to_erase_bufs(num_threads);
  if (res == kEtcPalErrOk)
    res = init_recv_thread_context_buf(num_threads);
  if (res == kEtcPalErrOk)
    res = init_universe_data_buf(num_threads);
  if (res == kEtcPalErrOk)
    res = init_sources_lost_bufs(num_threads);
  if (res == kEtcPalErrOk)
    res = init_source_pap_lost_buf(num_threads);
  if (res == kEtcPalErrOk)
    res = init_sampling_started_bufs(num_threads);
  if (res == kEtcPalErrOk)
    res = init_sampling_ended_bufs(num_threads);
  if (res == kEtcPalErrOk)
    res = init_source_limit_exceeded_buf(num_threads);
#endif

  if (res == kEtcPalErrOk)
    res = init_sources();
  if (res == kEtcPalErrOk)
    res = init_receivers();

  // Clean up
  if (res != kEtcPalErrOk)
    sacn_mem_deinit();

  return res;
}

/*
 * Deinitialize the memory module and clean up any allocated memory.
 */
void sacn_mem_deinit(void)
{
  deinit_receivers();
  deinit_sources();

#if SACN_DYNAMIC_MEM
  deinit_source_limit_exceeded_buf();
  deinit_sampling_ended_bufs();
  deinit_sampling_started_bufs();
  deinit_source_pap_lost_buf();
  deinit_sources_lost_bufs();
  deinit_universe_data_buf();
  deinit_recv_thread_context_buf();
  deinit_to_erase_bufs();
  deinit_status_lists_buf();
#endif

  memset(&mem_bufs, 0, sizeof mem_bufs);
}

unsigned int sacn_mem_get_num_threads(void)
{
  return mem_bufs.num_threads;
}

/*
 * Get the SacnSourceStatusLists instance for a given thread. The instance will be initialized to
 * default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
SacnSourceStatusLists* get_status_lists(sacn_thread_id_t thread_id)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SacnSourceStatusLists* to_return = &mem_bufs.status_lists[thread_id];
    zero_status_lists(to_return);
    return to_return;
  }
  return NULL;
}

/*
 * Get a buffer of pointers to SacnTrackedSources to erase. The pointers will be initialized to
 * NULL.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be allocated.
 */
SacnTrackedSource** get_to_erase_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (thread_id < mem_bufs.num_threads)
  {
    ToEraseBuf* to_return = &mem_bufs.to_erase[thread_id];

    CHECK_CAPACITY(to_return, size, buf, SacnTrackedSource*, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, NULL);

    memset(to_return->buf, 0, size * sizeof(SacnTrackedSource*));
    return to_return->buf;
  }
  return NULL;
}

/*
 * Get a buffer to read incoming sACN data into for a given thread. The buffer will not be
 * initialized.
 *
 * Returns the buffer or NULL if the thread ID was invalid.
 */
SacnRecvThreadContext* get_recv_thread_context(sacn_thread_id_t thread_id)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SacnRecvThreadContext* to_return = &mem_bufs.recv_thread_context[thread_id];
    to_return->thread_id = thread_id;
    return to_return;
  }
  return NULL;
}

/*
 * Get the UniverseDataNotification instance for a given thread. The instance will be initialized
 * to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
UniverseDataNotification* get_universe_data(sacn_thread_id_t thread_id)
{
  if (thread_id < mem_bufs.num_threads)
  {
    UniverseDataNotification* to_return = &mem_bufs.universe_data[thread_id];
    memset(to_return, 0, sizeof(UniverseDataNotification));
    to_return->handle = SACN_RECEIVER_INVALID;
    return to_return;
  }
  return NULL;
}

/*
 * Get the SourcePapLostNotification instance for a given thread. The instance will be initialized
 * to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
SourcePapLostNotification* get_source_pap_lost(sacn_thread_id_t thread_id)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SourcePapLostNotification* to_return = &mem_bufs.source_pap_lost[thread_id];
    memset(to_return, 0, sizeof(SourcePapLostNotification));
    to_return->handle = SACN_RECEIVER_INVALID;
    return to_return;
  }
  return NULL;
}

/*
 * Get the SourceLimitExceededNotification instance for a given thread. The instance will be
 * initialized to default values.
 *
 * Returns the instance or NULL if the thread ID was invalid.
 */
SourceLimitExceededNotification* get_source_limit_exceeded(sacn_thread_id_t thread_id)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SourceLimitExceededNotification* to_return = &mem_bufs.source_limit_exceeded[thread_id];
    memset(to_return, 0, sizeof(SourceLimitExceededNotification));
    to_return->handle = SACN_RECEIVER_INVALID;
    return to_return;
  }
  return NULL;
}

/*
 * Get a buffer of SourcesLostNotification instances associated with a given thread. All instances
 * in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be allocated.
 */
SourcesLostNotification* get_sources_lost_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SourcesLostNotificationBuf* notifications = &mem_bufs.sources_lost[thread_id];

    // This one cannot use CHECK_CAPACITY() because of a special case in the initialization of the
    // reallocated buffer
#if SACN_DYNAMIC_MEM
    if (size > notifications->buf_capacity)
    {
      size_t new_capacity = grow_capacity(notifications->buf_capacity, size);
      SourcesLostNotification* new_buf =
          (SourcesLostNotification*)realloc(notifications->buf, new_capacity * sizeof(SourcesLostNotification));
      if (new_buf && init_sources_lost_array(&new_buf[notifications->buf_capacity],
                                             new_capacity - notifications->buf_capacity) == kEtcPalErrOk)
      {
        notifications->buf = new_buf;
        notifications->buf_capacity = new_capacity;
      }
      else
      {
        return NULL;
      }
    }
#else   // SACN_DYNAMIC_MEM
    if (size > SACN_RECEIVER_MAX_UNIVERSES)
      return NULL;
#endif  // SACN_DYNAMIC_MEM

    zero_sources_lost_array(notifications->buf, size);
    return notifications->buf;
  }
  return NULL;
}

/*
 * Get a buffer of SamplingStartedNotification instances associated with a given thread. All
 * instances in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be
 * allocated.
 */
SamplingStartedNotification* get_sampling_started_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SamplingStartedNotificationBuf* notifications = &mem_bufs.sampling_started[thread_id];

    CHECK_CAPACITY(notifications, size, buf, SamplingStartedNotification, SACN_RECEIVER_MAX_UNIVERSES, NULL);

    memset(notifications->buf, 0, size * sizeof(SamplingStartedNotification));
    for (size_t i = 0; i < size; ++i)
      notifications->buf[i].handle = SACN_RECEIVER_INVALID;

    return notifications->buf;
  }
  return NULL;
}

/*
 * Get a buffer of SamplingEndedNotification instances associated with a given thread. All
 * instances in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be
 * allocated.
 */
SamplingEndedNotification* get_sampling_ended_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SamplingEndedNotificationBuf* notifications = &mem_bufs.sampling_ended[thread_id];

    CHECK_CAPACITY(notifications, size, buf, SamplingEndedNotification, SACN_RECEIVER_MAX_UNIVERSES, NULL);

    memset(notifications->buf, 0, size * sizeof(SamplingEndedNotification));
    for (size_t i = 0; i < size; ++i)
      notifications->buf[i].handle = SACN_RECEIVER_INVALID;

    return notifications->buf;
  }
  return NULL;
}

/*
 * Add a new offline source to an SacnSourceStatusLists.
 *
 * [out] status_lists Status lists instance to which to append the new source.
 * [in] cid CID of the offline source.
 * [in] name Name of the offline source - just a reference to the name buffer stored with the
 *           corresponding SacnTrackedSource.
 * [in] terminated Whether the source was lost because its Stream_Terminated bit was set.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_offline_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name, bool terminated)
{
  SACN_ASSERT(status_lists);
  SACN_ASSERT(cid);

  CHECK_ROOM_FOR_ONE_MORE(status_lists, offline, SacnLostSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

  status_lists->offline[status_lists->num_offline].cid = *cid;
  status_lists->offline[status_lists->num_offline].name = name;
  status_lists->offline[status_lists->num_offline].terminated = terminated;
  ++status_lists->num_offline;
  return true;
}

/*
 * Add a new online source to an SacnSourceStatusLists.
 *
 * [out] status_lists Status lists instance to which to append the new source.
 * [in] cid CID of the online source.
 * [in] name Name of the online source - just a reference to the name buffer stored with the
 *           corresponding SacnTrackedSource.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_online_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name)
{
  SACN_ASSERT(status_lists);
  SACN_ASSERT(cid);

  CHECK_ROOM_FOR_ONE_MORE(status_lists, online, SacnRemoteSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE,
                          false);

  status_lists->online[status_lists->num_online].cid = *cid;
  status_lists->online[status_lists->num_online].name = name;
  ++status_lists->num_online;
  return true;
}

/*
 * Add a new unknown-status source to an SacnSourceStatusLists.
 *
 * [out] status_lists Status lists instance to which to append the new source.
 * [in] cid CID of the unknown-status source.
 * [in] name Name of the unknown-status source - just a reference to the name buffer stored with
 *           the corresponding SacnTrackedSource.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_unknown_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name)
{
  SACN_ASSERT(status_lists);
  SACN_ASSERT(cid);

  CHECK_ROOM_FOR_ONE_MORE(status_lists, unknown, SacnRemoteSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE,
                          false);

  status_lists->unknown[status_lists->num_unknown].cid = *cid;
  status_lists->unknown[status_lists->num_unknown].name = name;
  ++status_lists->num_unknown;
  return true;
}

/*
 * Add a new lost source to a SourcesLostNotification.
 *
 * [out] sources_lost SourcesLostNotification instance to which to append the lost source.
 * [in] cid CID of the source that was lost.
 * [in] name Name of the source that was lost; copied into the notification.
 * [in] terminated Whether the source was lost because its Stream_Terminated bit was set.
 * Returns true if the source was successfully added, false if memory could not be allocated.
 */
bool add_lost_source(SourcesLostNotification* sources_lost, const EtcPalUuid* cid, const char* name, bool terminated)
{
  SACN_ASSERT(sources_lost);
  SACN_ASSERT(cid);
  SACN_ASSERT(name);

  CHECK_ROOM_FOR_ONE_MORE(sources_lost, lost_sources, SacnLostSource, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE, false);

  sources_lost->lost_sources[sources_lost->num_lost_sources].cid = *cid;
  ETCPAL_MSVC_NO_DEP_WRN strcpy(sources_lost->lost_sources[sources_lost->num_lost_sources].name, name);
  sources_lost->lost_sources[sources_lost->num_lost_sources].terminated = terminated;
  ++sources_lost->num_lost_sources;

  return true;
}

/*
 * Add a new dead socket to a SacnRecvThreadContext.
 *
 * [out] recv_thread_context SacnRecvThreadConstext instance to which to append the dead socket.
 * [in] socket Dead socket.
 * Returns true if the socket was successfully added, false if memory could not be allocated.
 */
bool add_dead_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket)
{
  SACN_ASSERT(recv_thread_context);

  CHECK_ROOM_FOR_ONE_MORE(recv_thread_context, dead_sockets, etcpal_socket_t, SACN_RECEIVER_MAX_UNIVERSES * 2, false);

  recv_thread_context->dead_sockets[recv_thread_context->num_dead_sockets++] = socket;
  return true;
}

bool add_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket, etcpal_iptype_t ip_type,
                    bool bound)
{
#if !SACN_RECEIVER_LIMIT_BIND
  ETCPAL_UNUSED_ARG(bound);
#endif

  SACN_ASSERT(recv_thread_context);

  CHECK_ROOM_FOR_ONE_MORE(recv_thread_context, socket_refs, SocketRef, SACN_RECEIVER_MAX_SOCKET_REFS, false);

  recv_thread_context->socket_refs[recv_thread_context->num_socket_refs].sock = socket;
  recv_thread_context->socket_refs[recv_thread_context->num_socket_refs].refcount = 1;
  recv_thread_context->socket_refs[recv_thread_context->num_socket_refs].ip_type = ip_type;
#if SACN_RECEIVER_LIMIT_BIND
  recv_thread_context->socket_refs[recv_thread_context->num_socket_refs].bound = bound;
#endif
  ++recv_thread_context->num_socket_refs;
  ++recv_thread_context->new_socket_refs;
  return true;
}

bool remove_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket)
{
  SACN_ASSERT(recv_thread_context);

  for (size_t i = 0; i < recv_thread_context->num_socket_refs; ++i)
  {
    SocketRef* ref = &recv_thread_context->socket_refs[i];
    if (ref->sock == socket)
    {
      if (--ref->refcount == 0)
      {
        if (i < recv_thread_context->num_socket_refs - 1)
          memmove(ref, ref + 1, (recv_thread_context->num_socket_refs - 1 - i) * sizeof(SocketRef));
        --recv_thread_context->num_socket_refs;
        return true;
      }
    }
  }
  return false;
}

void add_receiver_to_list(SacnRecvThreadContext* recv_thread_context, SacnReceiver* receiver)
{
  SacnReceiver* list_entry = recv_thread_context->receivers;
  while (list_entry && list_entry->next)
    list_entry = list_entry->next;

  if (list_entry)
    list_entry->next = receiver;
  else
    recv_thread_context->receivers = receiver;

  ++recv_thread_context->num_receivers;
}

void remove_receiver_from_list(SacnRecvThreadContext* recv_thread_context, SacnReceiver* receiver)
{
  SacnReceiver* last = NULL;
  SacnReceiver* entry = recv_thread_context->receivers;
  while (entry)
  {
    if (entry == receiver)
    {
      if (!last)
      {
        // replace the head
        recv_thread_context->receivers = entry->next;
      }
      else
      {
        last->next = entry->next;
      }

      if (recv_thread_context->num_receivers > 0)
        --recv_thread_context->num_receivers;

      receiver->next = NULL;
      break;
    }
    last = entry;
    entry = entry->next;
  }
}

// Needs lock
etcpal_error_t add_sacn_source(sacn_source_t handle, const SacnSourceConfig* config, SacnSource** source_state)
{
#if SACN_SOURCE_ENABLED
  etcpal_error_t result = kEtcPalErrOk;
  SacnSource* source = NULL;
  if (lookup_source(handle, &source) == kEtcPalErrOk)
    result = kEtcPalErrExists;

  if (result == kEtcPalErrOk)
  {
    CHECK_ROOM_FOR_ONE_MORE((&mem_bufs), sources, SacnSource, SACN_SOURCE_MAX_SOURCES, kEtcPalErrNoMem);

    source = &mem_bufs.sources[mem_bufs.num_sources];
  }

  if (result == kEtcPalErrOk)
  {
    source->handle = handle;

    // Initialize the universe discovery send buffer.
    memset(source->universe_discovery_send_buf, 0, SACN_MTU);

    int written = 0;
    written += pack_sacn_root_layer(source->universe_discovery_send_buf, SACN_UNIVERSE_DISCOVERY_HEADER_SIZE, true,
                                    &config->cid);
    written +=
        pack_sacn_universe_discovery_framing_layer(&source->universe_discovery_send_buf[written], 0, config->name);
    written += pack_sacn_universe_discovery_layer_header(&source->universe_discovery_send_buf[written], 0, 0, 0);

    // Initialize everything else.
    source->cid = config->cid;
    memset(source->name, 0, SACN_SOURCE_NAME_MAX_LEN);
    memcpy(source->name, config->name, strlen(config->name));
    source->terminating = false;
    source->num_active_universes = 0;
    etcpal_timer_start(&source->universe_discovery_timer, SACN_UNIVERSE_DISCOVERY_INTERVAL);
    source->process_manually = config->manually_process_source;
    source->ip_supported = config->ip_supported;
    source->keep_alive_interval = config->keep_alive_interval;
    source->universe_count_max = config->universe_count_max;

    source->num_universes = 0;
    source->num_netints = 0;
#if SACN_DYNAMIC_MEM
    source->universes = calloc(INITIAL_CAPACITY, sizeof(SacnSourceUniverse));
    source->universes_capacity = source->universes ? INITIAL_CAPACITY : 0;
    source->netints = calloc(INITIAL_CAPACITY, sizeof(SacnSourceNetint));
    source->netints_capacity = source->netints ? INITIAL_CAPACITY : 0;

    if (!source->universes || !source->netints)
      result = kEtcPalErrNoMem;
#endif
  }

  if (result == kEtcPalErrOk)
  {
    ++mem_bufs.num_sources;
  }
  else
  {
#if SACN_DYNAMIC_MEM
    if (source)
    {
      if (source->universes)
      {
        free(source->universes);
        source->universes = NULL;
      }

      if (source->netints)
      {
        free(source->netints);
        source->netints = NULL;
      }
    }
#endif  // SACN_DYNAMIC_MEM
  }

  *source_state = source;

  return result;
#else   // SACN_SOURCE_ENABLED
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(source_state);
  return kEtcPalErrNotImpl;
#endif  // SACN_SOURCE_ENABLED
}

// Needs lock
etcpal_error_t add_sacn_source_universe(SacnSource* source, const SacnSourceUniverseConfig* config,
                                        SacnMcastInterface* netints, size_t num_netints,
                                        SacnSourceUniverse** universe_state)
{
  etcpal_error_t result = kEtcPalErrOk;

#if SACN_DYNAMIC_MEM
  // Make sure to check against universe_count_max.
  if ((source->universe_count_max != SACN_SOURCE_INFINITE_UNIVERSES) &&
      (source->num_universes >= source->universe_count_max))
  {
    result = kEtcPalErrNoMem;  // No room to allocate additional universe.
  }
#endif

  SacnSourceUniverse* universe = NULL;
  if (result == kEtcPalErrOk)
  {
    if (lookup_universe(source, config->universe, &universe) == kEtcPalErrOk)
      result = kEtcPalErrExists;
  }

  if (result == kEtcPalErrOk)
  {
    CHECK_ROOM_FOR_ONE_MORE(source, universes, SacnSourceUniverse, SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE,
                            kEtcPalErrNoMem);

    universe = &source->universes[source->num_universes];

    universe->universe_id = config->universe;

    universe->terminating = false;
    universe->num_terminations_sent = 0;

    universe->priority = config->priority;
    universe->sync_universe = config->sync_universe;
    universe->send_preview = config->send_preview;
    universe->seq_num = 0;

    universe->level_packets_sent_before_suppression = 0;
    init_sacn_data_send_buf(universe->level_send_buf, 0x00, &source->cid, source->name, config->priority,
                            config->universe, config->sync_universe, config->send_preview);
    universe->has_level_data = false;

#if SACN_ETC_PRIORITY_EXTENSION
    universe->pap_packets_sent_before_suppression = 0;
    init_sacn_data_send_buf(universe->pap_send_buf, 0xDD, &source->cid, source->name, config->priority,
                            config->universe, config->sync_universe, config->send_preview);
    universe->has_pap_data = false;
#endif

    universe->send_unicast_only = config->send_unicast_only;

    universe->num_unicast_dests = 0;
#if SACN_DYNAMIC_MEM
    universe->unicast_dests = calloc(INITIAL_CAPACITY, sizeof(SacnUnicastDestination));
    universe->unicast_dests_capacity = universe->unicast_dests ? INITIAL_CAPACITY : 0;

    if (!universe->unicast_dests)
      result = kEtcPalErrNoMem;
#endif
  }

  for (size_t i = 0; (result == kEtcPalErrOk) && (i < config->num_unicast_destinations); ++i)
  {
    SacnUnicastDestination* dest = NULL;
    result = add_sacn_unicast_dest(universe, &config->unicast_destinations[i], &dest);

    if (result == kEtcPalErrExists)
      result = kEtcPalErrOk;  // Duplicates are automatically filtered and not a failure condition.
  }

  if (result == kEtcPalErrOk)
    result = sacn_initialize_source_netints(&universe->netints, netints, num_netints);

  if (result == kEtcPalErrOk)
  {
    ++source->num_universes;
  }
  else
  {
#if SACN_DYNAMIC_MEM
    if (universe->netints.netints)
    {
      free(universe->netints.netints);
      universe->netints.netints = NULL;
    }

    if (universe->unicast_dests)
    {
      free(universe->unicast_dests);
      universe->unicast_dests = NULL;
    }
#endif
  }

  *universe_state = universe;

  return result;
}

// Needs lock
etcpal_error_t add_sacn_unicast_dest(SacnSourceUniverse* universe, const EtcPalIpAddr* addr,
                                     SacnUnicastDestination** dest_state)
{
  etcpal_error_t result = kEtcPalErrOk;
  SacnUnicastDestination* dest = NULL;

  if (lookup_unicast_dest(universe, addr, &dest) == kEtcPalErrOk)
    result = kEtcPalErrExists;

  if (result == kEtcPalErrOk)
  {
    CHECK_ROOM_FOR_ONE_MORE(universe, unicast_dests, SacnUnicastDestination, SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE,
                            kEtcPalErrNoMem);

    dest = &universe->unicast_dests[universe->num_unicast_dests++];
    dest->dest_addr = *addr;
    dest->terminating = false;
    dest->num_terminations_sent = 0;
  }

  *dest_state = dest;

  return result;
}

// Needs lock
etcpal_error_t add_sacn_source_netint(SacnSource* source, const EtcPalMcastNetintId* id)
{
  SacnSourceNetint* netint = lookup_source_netint(source, id);

  if (netint)
  {
    ++netint->num_refs;
  }
  else
  {
    CHECK_ROOM_FOR_ONE_MORE(source, netints, SacnSourceNetint, SACN_MAX_NETINTS, kEtcPalErrNoMem);

    netint = &source->netints[source->num_netints++];
    netint->id = *id;
    netint->num_refs = 1;
  }

  return kEtcPalErrOk;
}

// Needs lock
etcpal_error_t lookup_source_and_universe(sacn_source_t source, uint16_t universe, SacnSource** source_state,
                                          SacnSourceUniverse** universe_state)
{
  etcpal_error_t result = lookup_source(source, source_state);

  if (result == kEtcPalErrOk)
    result = lookup_universe(*source_state, universe, universe_state);

  return result;
}

// Needs lock
etcpal_error_t lookup_source(sacn_source_t handle, SacnSource** source_state)
{
#if SACN_SOURCE_ENABLED
  bool found = false;
  size_t index = get_source_index(handle, &found);
  *source_state = found ? &mem_bufs.sources[index] : NULL;
  return found ? kEtcPalErrOk : kEtcPalErrNotFound;
#else
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(source_state);
  return kEtcPalErrNotFound;
#endif
}

// Needs lock
etcpal_error_t lookup_universe(SacnSource* source, uint16_t universe, SacnSourceUniverse** universe_state)
{
  bool found = false;
  size_t index = get_source_universe_index(source, universe, &found);
  *universe_state = found ? &source->universes[index] : NULL;
  return found ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
etcpal_error_t lookup_unicast_dest(SacnSourceUniverse* universe, const EtcPalIpAddr* addr,
                                   SacnUnicastDestination** unicast_dest)
{
  bool found = false;
  size_t index = get_unicast_dest_index(universe, addr, &found);
  *unicast_dest = found ? &universe->unicast_dests[index] : NULL;
  return found ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
SacnSourceNetint* lookup_source_netint(SacnSource* source, const EtcPalMcastNetintId* id)
{
  bool found = false;
  size_t index = get_source_netint_index(source, id, &found);
  return found ? &source->netints[index] : NULL;
}

// Needs lock
SacnSourceNetint* lookup_source_netint_and_index(SacnSource* source, const EtcPalMcastNetintId* id, size_t* index)
{
  bool found = false;
  *index = get_source_netint_index(source, id, &found);
  return found ? &source->netints[*index] : NULL;
}

SacnSource* get_source(size_t index)
{
#if SACN_SOURCE_ENABLED
  return (index < mem_bufs.num_sources) ? &mem_bufs.sources[index] : NULL;
#else
  ETCPAL_UNUSED_ARG(index);
  return NULL;
#endif
}

size_t get_num_sources()
{
#if SACN_SOURCE_ENABLED
  return mem_bufs.num_sources;
#else
  return 0;
#endif
}

// Needs lock
void remove_sacn_source_netint(SacnSource* source, size_t index)
{
  REMOVE_AT_INDEX(source, SacnSourceNetint, netints, index);
}

// Needs lock
void remove_sacn_unicast_dest(SacnSourceUniverse* universe, size_t index)
{
  REMOVE_AT_INDEX(universe, SacnUnicastDestination, unicast_dests, index);
}

// Needs lock
void remove_sacn_source_universe(SacnSource* source, size_t index)
{
#if SACN_DYNAMIC_MEM
  if (source->universes[index].unicast_dests)
    free(source->universes[index].unicast_dests);
  if (source->universes[index].netints.netints)
    free(source->universes[index].netints.netints);
#endif
  REMOVE_AT_INDEX(source, SacnSourceUniverse, universes, index);
}

// Needs lock
void remove_sacn_source(size_t index)
{
#if SACN_SOURCE_ENABLED
#if SACN_DYNAMIC_MEM
  if (mem_bufs.sources[index].universes)
    free(mem_bufs.sources[index].universes);
  if (mem_bufs.sources[index].netints)
    free(mem_bufs.sources[index].netints);
#endif  // SACN_DYNAMIC_MEM

  REMOVE_AT_INDEX((&mem_bufs), SacnSource, sources, index);
#else   // SACN_SOURCE_ENABLED
  ETCPAL_UNUSED_ARG(index);
#endif  // SACN_SOURCE_ENABLED
}

/*
 * Allocate a new receiver instances and do essential first initialization, in preparation for
 * creating the sockets and subscriptions.
 *
 * [in] config Receiver configuration data.
 * Returns the new initialized receiver instance, or NULL if out of memory.
 */
etcpal_error_t add_sacn_receiver(sacn_receiver_t handle, const SacnReceiverConfig* config, SacnMcastInterface* netints,
                                 size_t num_netints, SacnReceiver** receiver_state)
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

  etcpal_error_t initialize_receiver_netints_result =
      sacn_initialize_receiver_netints(&receiver->netints, netints, num_netints);
  if (initialize_receiver_netints_result != kEtcPalErrOk)
  {
    FREE_RECEIVER(receiver);
    return initialize_receiver_netints_result;
  }

  receiver->sampling = false;
  receiver->notified_sampling_started = false;
  receiver->suppress_limit_exceeded_notification = false;
  etcpal_rbtree_init(&receiver->sources, tracked_source_compare, node_alloc, node_dealloc);
  receiver->term_sets = NULL;

  receiver->filter_preview_data = ((config->flags & SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA) != 0);

  receiver->callbacks = config->callbacks;

  receiver->source_count_max = config->source_count_max;

  receiver->ip_supported = config->ip_supported;

  receiver->next = NULL;

  *receiver_state = receiver;

  // Insert the new universe into the map.
  return insert_receiver_into_maps(receiver);
}

etcpal_error_t add_sacn_tracked_source(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const char* name,
                                       uint8_t seq_num, uint8_t first_start_code,
                                       SacnTrackedSource** tracked_source_state)
{
  etcpal_error_t result = kEtcPalErrOk;
  SacnTrackedSource* src = NULL;

  size_t current_number_of_sources = etcpal_rbtree_size(&receiver->sources);
#if SACN_DYNAMIC_MEM
  size_t max_number_of_sources = receiver->source_count_max;
  bool infinite_sources = (max_number_of_sources == SACN_RECEIVER_INFINITE_SOURCES);
#else
  size_t max_number_of_sources = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE;
  bool infinite_sources = false;
#endif

  if (infinite_sources || (current_number_of_sources < max_number_of_sources))
    src = ALLOC_TRACKED_SOURCE();

  if (!src)
    result = kEtcPalErrNoMem;

  if (result == kEtcPalErrOk)
  {
    src->cid = *sender_cid;
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

    etcpal_rbtree_insert(&receiver->sources, src);
    *tracked_source_state = src;
  }

  return result;
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

etcpal_error_t clear_receiver_sources(SacnReceiver* receiver)
{
  receiver->suppress_limit_exceeded_notification = false;
  return etcpal_rbtree_clear_with_cb(&receiver->sources, source_tree_dealloc);
}

etcpal_error_t remove_receiver_source(SacnReceiver* receiver, const EtcPalUuid* cid)
{
  return etcpal_rbtree_remove_with_cb(&receiver->sources, cid, source_tree_dealloc);
}

void remove_sacn_receiver(SacnReceiver* receiver)
{
  etcpal_rbtree_clear_with_cb(&receiver->sources, source_tree_dealloc);
  remove_receiver_from_maps(receiver);
  FREE_RECEIVER(receiver);
}

void zero_status_lists(SacnSourceStatusLists* status_lists)
{
  SACN_ASSERT(status_lists);

  status_lists->num_online = 0;
  status_lists->num_offline = 0;
  status_lists->num_unknown = 0;
}

void zero_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size)
{
  for (SourcesLostNotification* sources_lost = sources_lost_arr; sources_lost < sources_lost_arr + size; ++sources_lost)
  {
    sources_lost->callback = NULL;
    sources_lost->handle = SACN_RECEIVER_INVALID;
    sources_lost->num_lost_sources = 0;
    sources_lost->context = NULL;
  }
}

size_t get_source_index(sacn_source_t handle, bool* found)
{
  *found = false;
  size_t index = 0;

#if SACN_SOURCE_ENABLED
  while (!(*found) && (index < mem_bufs.num_sources))
  {
    if (mem_bufs.sources[index].handle == handle)
      *found = true;
    else
      ++index;
  }
#else
  ETCPAL_UNUSED_ARG(handle);
#endif

  return index;
}

size_t get_source_universe_index(SacnSource* source, uint16_t universe, bool* found)
{
  *found = false;
  size_t index = 0;

  while (!(*found) && (index < source->num_universes))
  {
    if (source->universes[index].universe_id == universe)
      *found = true;
    else
      ++index;
  }

  return index;
}

size_t get_unicast_dest_index(SacnSourceUniverse* universe, const EtcPalIpAddr* addr, bool* found)
{
  *found = false;
  size_t index = 0;

  while (!(*found) && (index < universe->num_unicast_dests))
  {
    if (etcpal_ip_cmp(&universe->unicast_dests[index].dest_addr, addr) == 0)
      *found = true;
    else
      ++index;
  }

  return index;
}

size_t get_source_netint_index(SacnSource* source, const EtcPalMcastNetintId* id, bool* found)
{
  *found = false;
  size_t index = 0;

  while (!(*found) && (index < source->num_netints))
  {
    if ((source->netints[index].id.index == id->index) && (source->netints[index].id.ip_type == id->ip_type))
      *found = true;
    else
      ++index;
  }

  return index;
}

#if SACN_DYNAMIC_MEM
size_t grow_capacity(size_t old_capacity, size_t capacity_requested)
{
  size_t capacity = old_capacity;
  while (capacity < capacity_requested)
    capacity *= 2;
  return capacity;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Dynamic memory/buffer initialization functions
///////////////////////////////////////////////////////////////////////////////

#if SACN_DYNAMIC_MEM

etcpal_error_t init_status_lists_buf(unsigned int num_threads)
{
  mem_bufs.status_lists = calloc(num_threads, sizeof(SacnSourceStatusLists));
  if (!mem_bufs.status_lists)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_status_lists_entry(&mem_bufs.status_lists[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_status_lists_entry(SacnSourceStatusLists* status_lists)
{
  SACN_ASSERT(status_lists);

  etcpal_error_t res = kEtcPalErrOk;

  status_lists->offline = calloc(INITIAL_CAPACITY, sizeof(SacnLostSourceInternal));
  if (status_lists->offline)
    status_lists->offline_capacity = INITIAL_CAPACITY;
  else
    res = kEtcPalErrNoMem;

  if (res == kEtcPalErrOk)
  {
    status_lists->online = calloc(INITIAL_CAPACITY, sizeof(SacnRemoteSourceInternal));
    if (status_lists->online)
      status_lists->online_capacity = INITIAL_CAPACITY;
    else
      res = kEtcPalErrNoMem;
  }

  if (res == kEtcPalErrOk)
  {
    status_lists->unknown = calloc(INITIAL_CAPACITY, sizeof(SacnRemoteSourceInternal));
    if (status_lists->unknown)
      status_lists->unknown_capacity = INITIAL_CAPACITY;
    else
      res = kEtcPalErrNoMem;
  }

  return res;
}

etcpal_error_t init_to_erase_bufs(unsigned int num_threads)
{
  mem_bufs.to_erase = calloc(num_threads, sizeof(ToEraseBuf));
  if (!mem_bufs.to_erase)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_to_erase_buf(&mem_bufs.to_erase[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_to_erase_buf(ToEraseBuf* to_erase_buf)
{
  SACN_ASSERT(to_erase_buf);

  to_erase_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SacnTrackedSource*));
  if (!to_erase_buf->buf)
    return kEtcPalErrNoMem;

  to_erase_buf->buf_capacity = INITIAL_CAPACITY;
  return kEtcPalErrOk;
}

etcpal_error_t init_recv_thread_context_buf(unsigned int num_threads)
{
  mem_bufs.recv_thread_context = calloc(num_threads, sizeof(SacnRecvThreadContext));
  if (!mem_bufs.recv_thread_context)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_recv_thread_context_entry(&mem_bufs.recv_thread_context[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_recv_thread_context_entry(SacnRecvThreadContext* recv_thread_context)
{
  SACN_ASSERT(recv_thread_context);

  recv_thread_context->dead_sockets = calloc(INITIAL_CAPACITY, sizeof(etcpal_socket_t));
  if (!recv_thread_context->dead_sockets)
    return kEtcPalErrNoMem;
  recv_thread_context->dead_sockets_capacity = INITIAL_CAPACITY;

  recv_thread_context->socket_refs = calloc(INITIAL_CAPACITY, sizeof(SocketRef));
  if (!recv_thread_context->socket_refs)
    return kEtcPalErrNoMem;
  recv_thread_context->socket_refs_capacity = INITIAL_CAPACITY;

  recv_thread_context->num_socket_refs = 0;
  recv_thread_context->new_socket_refs = 0;
#if SACN_RECEIVER_LIMIT_BIND
  recv_thread_context->ipv4_bound = false;
  recv_thread_context->ipv6_bound = false;
#endif

  return kEtcPalErrOk;
}

etcpal_error_t init_universe_data_buf(unsigned int num_threads)
{
  mem_bufs.universe_data = calloc(num_threads, sizeof(UniverseDataNotification));
  if (!mem_bufs.universe_data)
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

etcpal_error_t init_sources_lost_bufs(unsigned int num_threads)
{
  mem_bufs.sources_lost = calloc(num_threads, sizeof(SourcesLostNotificationBuf));
  if (!mem_bufs.sources_lost)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sources_lost_buf(&mem_bufs.sources_lost[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf)
{
  SACN_ASSERT(sources_lost_buf);

  sources_lost_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SourcesLostNotification));
  if (!sources_lost_buf->buf)
    return kEtcPalErrNoMem;

  sources_lost_buf->buf_capacity = INITIAL_CAPACITY;
  return init_sources_lost_array(sources_lost_buf->buf, INITIAL_CAPACITY);
}

etcpal_error_t init_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size)
{
  SACN_ASSERT(sources_lost_arr);

  for (SourcesLostNotification* sources_lost = sources_lost_arr; sources_lost < sources_lost_arr + size; ++sources_lost)
  {
    sources_lost->lost_sources = calloc(INITIAL_CAPACITY, sizeof(SacnLostSource));
    if (!sources_lost->lost_sources)
      return kEtcPalErrNoMem;
    sources_lost->lost_sources_capacity = INITIAL_CAPACITY;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sampling_started_bufs(unsigned int num_threads)
{
  mem_bufs.sampling_started = calloc(num_threads, sizeof(SamplingStartedNotificationBuf));
  if (!mem_bufs.sampling_started)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sampling_started_buf(&mem_bufs.sampling_started[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf)
{
  SACN_ASSERT(sampling_started_buf);

  sampling_started_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SamplingStartedNotification));
  if (!sampling_started_buf->buf)
    return kEtcPalErrNoMem;

  sampling_started_buf->buf_capacity = INITIAL_CAPACITY;
  return kEtcPalErrOk;
}

etcpal_error_t init_sampling_ended_bufs(unsigned int num_threads)
{
  mem_bufs.sampling_ended = calloc(num_threads, sizeof(SamplingEndedNotificationBuf));
  if (!mem_bufs.sampling_ended)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sampling_ended_buf(&mem_bufs.sampling_ended[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf)
{
  SACN_ASSERT(sampling_ended_buf);

  sampling_ended_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SamplingEndedNotification));
  if (!sampling_ended_buf->buf)
    return kEtcPalErrNoMem;

  sampling_ended_buf->buf_capacity = INITIAL_CAPACITY;
  return kEtcPalErrOk;
}

etcpal_error_t init_source_pap_lost_buf(unsigned int num_threads)
{
  mem_bufs.source_pap_lost = calloc(num_threads, sizeof(SourcePapLostNotification));
  if (!mem_bufs.source_pap_lost)
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

etcpal_error_t init_source_limit_exceeded_buf(unsigned int num_threads)
{
  mem_bufs.source_limit_exceeded = calloc(num_threads, sizeof(SourceLimitExceededNotification));
  if (!mem_bufs.source_limit_exceeded)
    return kEtcPalErrNoMem;
  return kEtcPalErrOk;
}

///////////////////////////////////////////////////////////////////////////////
// Dynamic memory/buffer deinitialization functions
///////////////////////////////////////////////////////////////////////////////

void deinit_status_lists_buf(void)
{
  if (mem_bufs.status_lists)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_status_lists_entry(&mem_bufs.status_lists[i]);
    free(mem_bufs.status_lists);
  }
}

void deinit_status_lists_entry(SacnSourceStatusLists* status_lists)
{
  SACN_ASSERT(status_lists);

  if (status_lists->offline)
    free(status_lists->offline);
  if (status_lists->online)
    free(status_lists->online);
  if (status_lists->unknown)
    free(status_lists->unknown);
}

void deinit_to_erase_bufs(void)
{
  if (mem_bufs.to_erase)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_to_erase_buf(&mem_bufs.to_erase[i]);
    free(mem_bufs.to_erase);
  }
}

void deinit_to_erase_buf(ToEraseBuf* to_erase_buf)
{
  SACN_ASSERT(to_erase_buf);

  if (to_erase_buf->buf)
    free(to_erase_buf->buf);
}

void deinit_recv_thread_context_buf(void)
{
  if (mem_bufs.recv_thread_context)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_recv_thread_context_entry(&mem_bufs.recv_thread_context[i]);

    free(mem_bufs.recv_thread_context);
  }
}

void deinit_recv_thread_context_entry(SacnRecvThreadContext* recv_thread_context)
{
  SACN_ASSERT(recv_thread_context);

  if (recv_thread_context->dead_sockets)
    free(recv_thread_context->dead_sockets);
  if (recv_thread_context->socket_refs)
    free(recv_thread_context->socket_refs);
}

void deinit_universe_data_buf(void)
{
  if (mem_bufs.universe_data)
    free(mem_bufs.universe_data);
}

void deinit_sources_lost_bufs(void)
{
  if (mem_bufs.sources_lost)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_sources_lost_buf(&mem_bufs.sources_lost[i]);
    free(mem_bufs.sources_lost);
  }
}

void deinit_sources_lost_buf(SourcesLostNotificationBuf* sources_lost_buf)
{
  SACN_ASSERT(sources_lost_buf);

  if (sources_lost_buf->buf)
  {
    for (size_t i = 0; i < sources_lost_buf->buf_capacity; ++i)
    {
      deinit_sources_lost_entry(&sources_lost_buf->buf[i]);
    }
    free(sources_lost_buf->buf);
  }
}

void deinit_sources_lost_entry(SourcesLostNotification* sources_lost)
{
  SACN_ASSERT(sources_lost);

  if (sources_lost->lost_sources)
    free(sources_lost->lost_sources);
}

void deinit_source_pap_lost_buf(void)
{
  if (mem_bufs.source_pap_lost)
    free(mem_bufs.source_pap_lost);
}

void deinit_sampling_started_bufs(void)
{
  if (mem_bufs.sampling_started)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_sampling_started_buf(&mem_bufs.sampling_started[i]);
    free(mem_bufs.sampling_started);
  }
}

void deinit_sampling_started_buf(SamplingStartedNotificationBuf* sampling_started_buf)
{
  SACN_ASSERT(sampling_started_buf);

  if (sampling_started_buf->buf)
    free(sampling_started_buf->buf);
}

void deinit_sampling_ended_bufs(void)
{
  if (mem_bufs.sampling_ended)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_sampling_ended_buf(&mem_bufs.sampling_ended[i]);
    free(mem_bufs.sampling_ended);
  }
}

void deinit_sampling_ended_buf(SamplingEndedNotificationBuf* sampling_ended_buf)
{
  SACN_ASSERT(sampling_ended_buf);

  if (sampling_ended_buf->buf)
    free(sampling_ended_buf->buf);
}

void deinit_source_limit_exceeded_buf(void)
{
  if (mem_bufs.source_limit_exceeded)
    free(mem_bufs.source_limit_exceeded);
}

#endif  // SACN_DYNAMIC_MEM

/**************************************************************************************************
 * Internal helpers for managing the receivers, the sources they track, and their trees
 *************************************************************************************************/

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

int tracked_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnTrackedSource* a = (const SacnTrackedSource*)value_a;
  const SacnTrackedSource* b = (const SacnTrackedSource*)value_b;
  return ETCPAL_UUID_CMP(&a->cid, &b->cid);
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

EtcPalRbNode* node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacnrecv_rb_nodes);
#endif
}

void node_dealloc(EtcPalRbNode* node)
{
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacnrecv_rb_nodes, node);
#endif
}

/* Helper function for clearing an EtcPalRbTree containing sources. */
static void source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);
  FREE_TRACKED_SOURCE(node->value);
  node_dealloc(node);
}

/* Helper function for clearing an EtcPalRbTree containing SacnReceivers. */
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  SacnReceiver* receiver = (SacnReceiver*)node->value;
  etcpal_rbtree_clear_with_cb(&receiver->sources, source_tree_dealloc);
  FREE_RECEIVER(receiver);
  node_dealloc(node);
}

etcpal_error_t init_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;
#if SACN_SOURCE_ENABLED
#if SACN_DYNAMIC_MEM
  mem_bufs.sources = calloc(INITIAL_CAPACITY, sizeof(SacnSource));
  mem_bufs.sources_capacity = mem_bufs.sources ? INITIAL_CAPACITY : 0;
  if (!mem_bufs.sources)
    res = kEtcPalErrNoMem;
#endif  // SACN_DYNAMIC_MEM
  mem_bufs.num_sources = 0;

  if (res == kEtcPalErrOk)
    sources_initialized = true;
#endif  // SACN_SOURCE_ENABLED

  return res;
}

// Takes lock
void deinit_sources(void)
{
#if SACN_SOURCE_ENABLED
  if (sacn_lock())
  {
    if (sources_initialized)
    {
#if SACN_DYNAMIC_MEM
      free(mem_bufs.sources);
      mem_bufs.sources_capacity = 0;
#endif  // SACN_DYNAMIC_MEM
      mem_bufs.num_sources = 0;
      sources_initialized = false;
    }

    sacn_unlock();
  }
#endif  // SACN_SOURCE_ENABLED
}

etcpal_error_t init_receivers(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacnrecv_receivers);
  res |= etcpal_mempool_init(sacnrecv_tracked_sources);
  res |= etcpal_mempool_init(sacnrecv_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&receivers, receiver_compare, node_alloc, node_dealloc);
    etcpal_rbtree_init(&receivers_by_universe, receiver_compare_by_universe, node_alloc, node_dealloc);
  }

  return res;
}

void deinit_receivers(void)
{
  etcpal_rbtree_clear_with_cb(&receivers, universe_tree_dealloc);
  etcpal_rbtree_clear(&receivers_by_universe);
}
