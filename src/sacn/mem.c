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

#include "sacn/private/mem.h"

#include <stddef.h>
#include <string.h>
#include "etcpal/common.h"
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
#define UNIVERSE_DISCOVERY_INTERVAL 10000

/****************************** Private macros *******************************/

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

#endif  // SACN_DYNAMIC_MEM

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

#if !SACN_DYNAMIC_MEM && SACN_SOURCE_ENABLED
ETCPAL_MEMPOOL_DEFINE(sacnsource_source_states, SacnSource, SACN_SOURCE_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnsource_universe_states, SacnSourceUniverse,
                      (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE));
ETCPAL_MEMPOOL_DEFINE(sacnsource_netints, SacnSourceNetint, (SACN_SOURCE_MAX_SOURCES * SACN_MAX_NETINTS));
#if SACN_SOURCE_UNICAST_ENABLED
ETCPAL_MEMPOOL_DEFINE(sacnsource_unicast_dests, SacnUnicastDestination,
                      (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE *
                       SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE));
#endif
ETCPAL_MEMPOOL_DEFINE(sacnsource_rb_nodes, EtcPalRbNode,
                      SACN_SOURCE_MAX_SOURCES + (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE) +
                          (SACN_SOURCE_MAX_SOURCES * SACN_MAX_NETINTS) +
                          (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE *
                           SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE));
#endif

static EtcPalRbTree sources;
static bool sources_initialized = false;

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
#else
  SacnSourceStatusLists status_lists[SACN_RECEIVER_MAX_THREADS];
  ToEraseBuf to_erase[SACN_RECEIVER_MAX_THREADS];
  SacnRecvThreadContext recv_thread_context[SACN_RECEIVER_MAX_THREADS];

  UniverseDataNotification universe_data[SACN_RECEIVER_MAX_THREADS];
  SourcesLostNotificationBuf sources_lost[SACN_RECEIVER_MAX_THREADS];
  SourcePapLostNotification source_pap_lost[SACN_RECEIVER_MAX_THREADS];
  SamplingStartedNotificationBuf sampling_started[SACN_RECEIVER_MAX_THREADS];
  SamplingEndedNotificationBuf sampling_ended[SACN_RECEIVER_MAX_THREADS];
  SourceLimitExceededNotification source_limit_exceeded[SACN_RECEIVER_MAX_THREADS];
#endif
} mem_bufs;

/*********************** Private function prototypes *************************/

static int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int universe_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int netint_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int unicast_dests_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);

static EtcPalRbNode* source_rb_node_alloc_func(void);
static void source_rb_node_dealloc_func(EtcPalRbNode* node);
static void free_universes_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_netints_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_unicast_dests_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_sources_node(const EtcPalRbTree* self, EtcPalRbNode* node);

static void zero_status_lists(SacnSourceStatusLists* status_lists);
static void zero_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size);

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

// Sources initialization/deinitialization
static etcpal_error_t init_sources(void);
static void deinit_sources(void);

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
#else
    if (size > SACN_RECEIVER_MAX_UNIVERSES)
      return NULL;
#endif

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
  etcpal_error_t result = kEtcPalErrOk;
  SacnSource* source = ALLOC_SACN_SOURCE();

  if (source)
  {
    source->handle = handle;

    // Initialize the universe discovery send buffer.
    memset(source->universe_discovery_send_buf, 0, SACN_MTU);

    size_t written = 0;
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
    etcpal_timer_start(&source->universe_discovery_timer, UNIVERSE_DISCOVERY_INTERVAL);
    source->process_manually = config->manually_process_source;
    source->ip_supported = config->ip_supported;
    source->keep_alive_interval = config->keep_alive_interval;
    source->universe_count_max = config->universe_count_max;

    etcpal_rbtree_init(&source->universes, universe_state_lookup_compare_func, source_rb_node_alloc_func,
                       source_rb_node_dealloc_func);
    etcpal_rbtree_init(&source->netints, netint_state_lookup_compare_func, source_rb_node_alloc_func,
                       source_rb_node_dealloc_func);

    result = etcpal_rbtree_insert(&sources, source);
  }
  else
  {
    result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
    *source_state = source;
  else if (source)
    FREE_SACN_SOURCE(source);

  return result;
}

// Needs lock
etcpal_error_t add_sacn_source_universe(SacnSource* source, const SacnSourceUniverseConfig* config,
                                        SacnMcastInterface* netints, size_t num_netints,
                                        SacnSourceUniverse** universe_state)
{
  etcpal_error_t result = kEtcPalErrOk;
  SacnSourceUniverse* universe = ALLOC_SACN_SOURCE_UNIVERSE();

  if (universe)
  {
    // Initialize the universe's state.
    universe->universe_id = config->universe;
    etcpal_rbtree_init(&universe->unicast_dests, unicast_dests_lookup_compare_func, source_rb_node_alloc_func,
                       source_rb_node_dealloc_func);

    universe->terminating = false;
    universe->num_terminations_sent = 0;

    universe->priority = config->priority;
    universe->sync_universe = config->sync_universe;
    universe->send_preview = config->send_preview;
    universe->seq_num = 0;

    universe->null_packets_sent_before_suppression = 0;
    init_send_buf(universe->null_send_buf, 0x00, &source->cid, source->name, config->priority, config->universe,
                  config->sync_universe, config->send_preview);
    universe->has_null_data = false;

#if SACN_ETC_PRIORITY_EXTENSION
    universe->pap_packets_sent_before_suppression = 0;
    init_send_buf(universe->pap_send_buf, 0xDD, &source->cid, source->name, config->priority, config->universe,
                  config->sync_universe, config->send_preview);
    universe->has_pap_data = false;
#endif

    universe->send_unicast_only = config->send_unicast_only;

    for (size_t i = 0; (result == kEtcPalErrOk) && (i < config->num_unicast_destinations); ++i)
    {
      SacnUnicastDestination* dest = NULL;
      result = add_sacn_unicast_dest(universe, &config->unicast_destinations[i], &dest);

      if (result == kEtcPalErrExists)
        result = kEtcPalErrOk;  // Duplicates are automatically filtered and not a failure condition.
    }
  }
  else
  {
    result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
    result = sacn_initialize_internal_netints(&universe->netints, &universe->num_netints, netints, num_netints);

  if (result == kEtcPalErrOk)
    result = etcpal_rbtree_insert(&source->universes, universe);

  if (result == kEtcPalErrOk)
    *universe_state = universe;
  else if (universe)
    FREE_SACN_SOURCE_UNIVERSE(universe);

  return result;
}

// Needs lock
etcpal_error_t add_sacn_unicast_dest(SacnSourceUniverse* universe, const EtcPalIpAddr* addr,
                                     SacnUnicastDestination** dest_state)
{
  etcpal_error_t result = kEtcPalErrOk;
  SacnUnicastDestination* dest = ALLOC_SACN_UNICAST_DESTINATION();

  if (dest)
  {
    dest->dest_addr = *addr;
    dest->terminating = false;
    dest->num_terminations_sent = 0;
    result = etcpal_rbtree_insert(&universe->unicast_dests, dest);
  }
  else
  {
    result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
    *dest_state = dest;
  else if (dest)
    FREE_SACN_UNICAST_DESTINATION(dest);

  return result;
}

// Needs lock
etcpal_error_t add_sacn_source_netint(SacnSource* source, const EtcPalMcastNetintId* id,
                                      SacnSourceNetint** netint_state)
{
  etcpal_error_t result = kEtcPalErrOk;
  SacnSourceNetint* netint = ALLOC_SACN_SOURCE_NETINT();

  if (netint)
  {
    netint->id = *id;
    netint->num_refs = 1;
    result = etcpal_rbtree_insert(&source->netints, netint);
  }
  else
  {
    result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
    *netint_state = netint;
  else if (netint)
    FREE_SACN_SOURCE_NETINT(netint);

  return result;
}

// Needs lock
etcpal_error_t lookup_source_state(sacn_source_t source, uint16_t universe, SacnSource** source_state,
                                   SacnSourceUniverse** universe_state)
{
  etcpal_error_t result = kEtcPalErrOk;

  SacnSource* my_source_state = NULL;
  SacnSourceUniverse* my_universe_state = NULL;

  // Look up the source state.
  my_source_state = etcpal_rbtree_find(&sources, &source);

  if (!my_source_state)
    result = kEtcPalErrNotFound;

  // Look up the universe state.
  if ((result == kEtcPalErrOk) && universe_state)
  {
    my_universe_state = etcpal_rbtree_find(&my_source_state->universes, &universe);

    if (!my_universe_state)
      result = kEtcPalErrNotFound;
  }

  if (result == kEtcPalErrOk)
  {
    if (source_state)
      *source_state = my_source_state;
    if (universe_state)
      *universe_state = my_universe_state;
  }

  return result;
}

// Needs lock
etcpal_error_t lookup_unicast_dest(sacn_source_t source, uint16_t universe, const EtcPalIpAddr* addr,
                                   SacnUnicastDestination** unicast_dest)
{
  // Look up universe
  SacnSourceUniverse* universe_state = NULL;
  etcpal_error_t result = lookup_source_state(source, universe, NULL, &universe_state);

  // Validate
  if ((result == kEtcPalErrOk) && (!addr))
    result = kEtcPalErrSys;

  // Look up unicast destination
  SacnUnicastDestination* found = NULL;
  if (result == kEtcPalErrOk)
  {
    found = etcpal_rbtree_find(&universe_state->unicast_dests, addr);

    if (!found)
      result = kEtcPalErrNotFound;
  }

  // Pass back to application
  if ((result == kEtcPalErrOk) && unicast_dest)
    *unicast_dest = found;

  return result;
}

// Needs lock
SacnSourceNetint* lookup_source_netint(SacnSource* source, const EtcPalMcastNetintId* id)
{
  return (SacnSourceNetint*)etcpal_rbtree_find(&source->netints, id);
}

// Needs lock
EtcPalRbTree* get_sacn_sources()
{
  return &sources;
}

// Needs lock
void remove_sacn_source_netint(SacnSource* source, SacnSourceNetint** netint)
{
  if (etcpal_rbtree_remove(&source->netints, *netint) == kEtcPalErrOk)
  {
    FREE_SACN_SOURCE_NETINT(*netint);
    *netint = NULL;
  }
}

// Needs lock
void remove_sacn_unicast_dest(SacnSourceUniverse* universe, SacnUnicastDestination** dest, EtcPalRbIter* unicast_iter)
{
  SacnUnicastDestination* dest_to_remove = *dest;
  *dest = etcpal_rbiter_next(unicast_iter);
  etcpal_rbtree_remove(&universe->unicast_dests, dest_to_remove);
  FREE_SACN_UNICAST_DESTINATION(dest_to_remove);
}

// Needs lock
void remove_sacn_source_universe(SacnSource* source, SacnSourceUniverse** universe, EtcPalRbIter* universe_iter)
{
  SacnSourceUniverse* universe_to_remove = *universe;
  *universe = etcpal_rbiter_next(universe_iter);
  etcpal_rbtree_remove(&source->universes, universe_to_remove);
  FREE_SACN_SOURCE_UNIVERSE(universe_to_remove);
}

// Needs lock
void remove_sacn_source(SacnSource** source, EtcPalRbIter* source_iter)
{
  SacnSource* source_to_remove = *source;
  *source = etcpal_rbiter_next(source_iter);
  etcpal_rbtree_remove(&sources, source_to_remove);
  FREE_SACN_SOURCE(source_to_remove);
}

int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const SacnSource* a = (const SacnSource*)value_a;
  const SacnSource* b = (const SacnSource*)value_b;

  return (a->handle > b->handle) - (a->handle < b->handle);  // Just compare the handles.
}

int universe_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const SacnSourceUniverse* a = (const SacnSourceUniverse*)value_a;
  const SacnSourceUniverse* b = (const SacnSourceUniverse*)value_b;

  return (a->universe_id > b->universe_id) - (a->universe_id < b->universe_id);  // Just compare the IDs.
}

int netint_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const SacnSourceNetint* a = (const SacnSourceNetint*)value_a;
  const SacnSourceNetint* b = (const SacnSourceNetint*)value_b;

  return ((a->id.index > b->id.index) || ((a->id.index == b->id.index) && (a->id.ip_type > b->id.ip_type))) -
         ((a->id.index < b->id.index) || ((a->id.index == b->id.index) && (a->id.ip_type < b->id.ip_type)));
}

int unicast_dests_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const EtcPalIpAddr* a = (const EtcPalIpAddr*)value_a;
  const EtcPalIpAddr* b = (const EtcPalIpAddr*)value_b;

  bool greater_than = a->type > b->type;
  bool less_than = a->type < b->type;

  if (a->type == b->type)
  {
    if (a->type == kEtcPalIpTypeV4)
    {
      greater_than = a->addr.v4 > b->addr.v4;
      less_than = a->addr.v4 < b->addr.v4;
    }
    else if (a->type == kEtcPalIpTypeV6)
    {
      int cmp = memcmp(a->addr.v6.addr_buf, b->addr.v6.addr_buf, ETCPAL_IPV6_BYTES);
      greater_than = (cmp > 0);
      less_than = (cmp < 0);

      if (cmp == 0)
      {
        greater_than = a->addr.v6.scope_id > b->addr.v6.scope_id;
        less_than = a->addr.v6.scope_id < b->addr.v6.scope_id;
      }
    }
  }

  return (int)greater_than - (int)less_than;
}

EtcPalRbNode* source_rb_node_alloc_func(void)
{
  return ALLOC_SACN_SOURCE_RB_NODE();
}

void source_rb_node_dealloc_func(EtcPalRbNode* node)
{
  FREE_SACN_SOURCE_RB_NODE(node);
}

void free_universes_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SACN_SOURCE_UNIVERSE(node->value);
  FREE_SACN_SOURCE_RB_NODE(node);
}

void free_netints_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SACN_SOURCE_NETINT(node->value);
  FREE_SACN_SOURCE_RB_NODE(node);
}

void free_unicast_dests_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SACN_UNICAST_DESTINATION(node->value);
  FREE_SACN_SOURCE_RB_NODE(node);
}

void free_sources_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SACN_SOURCE(node->value);
  FREE_SACN_SOURCE_RB_NODE(node);
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

etcpal_error_t init_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if SACN_SOURCE_ENABLED
#if !SACN_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_source_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_universe_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_netints);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&sources, source_state_lookup_compare_func, source_rb_node_alloc_func,
                       source_rb_node_dealloc_func);
    sources_initialized = true;
  }
#endif

  return res;
}

// Takes lock
void deinit_sources(void)
{
  if (sacn_lock())
  {
    if (sources_initialized)
    {
      etcpal_rbtree_clear_with_cb(&sources, free_sources_node);
      sources_initialized = false;
    }

    sacn_unlock();
  }
}
