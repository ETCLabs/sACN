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

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#endif

/**************************** Private constants ******************************/

#define INITIAL_CAPACITY 8

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

#define CHECK_CAPACITY(container, size_requested, buffer, buffer_type, max_static)                              \
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
        return false;                                                                                           \
      }                                                                                                         \
    }                                                                                                           \
  } while (0)

#define CHECK_ROOM_FOR_ONE_MORE(container, buffer, buffer_type, max_static) \
  CHECK_CAPACITY(container, container->num_##buffer + 1, buffer, buffer_type, max_static)

#else  // SACN_DYNAMIC_MEM

#define CHECK_CAPACITY(container, size_requested, buffer, buffer_type, max_static) \
  do                                                                               \
  {                                                                                \
    if (size_requested > max_static)                                               \
    {                                                                              \
      return false;                                                                \
    }                                                                              \
  } while (0)

#define CHECK_ROOM_FOR_ONE_MORE(container, buffer, buffer_type, max_static) \
  CHECK_CAPACITY(container, container->num_##buffer + 1, buffer, buffer_type, max_static)

#endif  // SACN_DYNAMIC_MEM

/****************************** Private types ********************************/

typedef struct SourcesLostNotificationBuf
{
  SACN_DECLARE_BUF(SourcesLostNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SourcesLostNotificationBuf;

typedef struct SourcesFoundNotificationBuf
{
  SACN_DECLARE_BUF(SourcesFoundNotification, buf, SACN_RECEIVER_MAX_UNIVERSES);
} SourcesFoundNotificationBuf;

typedef struct ToEraseBuf
{
  SACN_DECLARE_BUF(SacnTrackedSource*, buf, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
} ToEraseBuf;

/**************************** Private variables ******************************/

static struct SacnMemBufs
{
  unsigned int num_threads;

#if SACN_DYNAMIC_MEM
  SacnSourceStatusLists* status_lists;
  ToEraseBuf* to_erase;
  SacnRecvThreadContext* recv_thread_context;

  UniverseDataNotification* universe_data;
  SourcesLostNotificationBuf* sources_lost;
  SourcesFoundNotificationBuf* sources_found;
  SourcePapLostNotification* source_pap_lost;
  SourceLimitExceededNotification* source_limit_exceeded;
#else
  SacnSourceStatusLists status_lists[SACN_RECEIVER_MAX_THREADS];
  ToEraseBuf to_erase[SACN_RECEIVER_MAX_THREADS];
  SacnRecvThreadContext recv_thread_context[SACN_RECEIVER_MAX_THREADS];

  UniverseDataNotification universe_data[SACN_RECEIVER_MAX_THREADS];
  SourcesLostNotificationBuf sources_lost[SACN_RECEIVER_MAX_THREADS];
  SourcesFoundNotificationBuf sources_found[SACN_RECEIVER_MAX_THREADS];
  SourcePapLostNotification source_pap_lost[SACN_RECEIVER_MAX_THREADS];
  SourceLimitExceededNotification source_limit_exceeded[SACN_RECEIVER_MAX_THREADS];
#endif
} mem_bufs;

/*********************** Private function prototypes *************************/

static void zero_status_lists(SacnSourceStatusLists* status_lists);
static void zero_sources_lost_array(SourcesLostNotification* sources_lost_arr, size_t size);
static void zero_sources_found_array(SourcesFoundNotification* sources_found_arr, size_t size);

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

static etcpal_error_t init_sources_found_bufs(unsigned int num_threads);
static etcpal_error_t init_sources_found_buf(SourcesFoundNotificationBuf* sources_found_buf);
static etcpal_error_t init_sources_found_array(SourcesFoundNotification* sources_found_arr, size_t size);

static etcpal_error_t init_source_pap_lost_buf(unsigned int num_threads);

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

static void deinit_sources_found_bufs(void);
static void deinit_sources_found_buf(SourcesFoundNotificationBuf* sources_found_buf);
static void deinit_sources_found_entry(SourcesFoundNotification* sources_found);

static void deinit_source_pap_lost_buf(void);

static void deinit_source_limit_exceeded_buf(void);
#endif  // SACN_DYNAMIC_MEM

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
    res = init_sources_found_bufs(num_threads);
  if (res == kEtcPalErrOk)
    res = init_source_pap_lost_buf(num_threads);
  if (res == kEtcPalErrOk)
    res = init_source_limit_exceeded_buf(num_threads);
#endif

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
#if SACN_DYNAMIC_MEM
  deinit_source_limit_exceeded_buf();
  deinit_source_pap_lost_buf();
  deinit_sources_lost_bufs();
  deinit_sources_found_bufs();
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

    CHECK_CAPACITY(to_return, size, buf, SacnTrackedSource*, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

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
 * Get a buffer of SourcesFoundNotification instances associated with a given thread. All instances
 * in the array will be initialized to default values.
 *
 * [in] thread_id Thread ID for which to get the buffer.
 * [in] size Size of the buffer requested.
 * Returns the buffer or NULL if the thread ID was invalid or memory could not be allocated.
 */
SourcesFoundNotification* get_sources_found_buffer(sacn_thread_id_t thread_id, size_t size)
{
  if (thread_id < mem_bufs.num_threads)
  {
    SourcesFoundNotificationBuf* notifications = &mem_bufs.sources_found[thread_id];

    // This one cannot use CHECK_CAPACITY() because of a special case in the initialization of the
    // reallocated buffer
#if SACN_DYNAMIC_MEM
    if (size > notifications->buf_capacity)
    {
      size_t new_capacity = grow_capacity(notifications->buf_capacity, size);
      SourcesFoundNotification* new_buf =
          (SourcesFoundNotification*)realloc(notifications->buf, new_capacity * sizeof(SourcesFoundNotification));
      if (new_buf && init_sources_found_array(&new_buf[notifications->buf_capacity],
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

    zero_sources_found_array(notifications->buf, size);
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

  CHECK_ROOM_FOR_ONE_MORE(status_lists, offline, SacnLostSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

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

  CHECK_ROOM_FOR_ONE_MORE(status_lists, online, SacnRemoteSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

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

  CHECK_ROOM_FOR_ONE_MORE(status_lists, unknown, SacnRemoteSourceInternal, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

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

  CHECK_ROOM_FOR_ONE_MORE(sources_lost, lost_sources, SacnLostSource, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

  sources_lost->lost_sources[sources_lost->num_lost_sources].cid = *cid;
  ETCPAL_MSVC_NO_DEP_WRN strcpy(sources_lost->lost_sources[sources_lost->num_lost_sources].name, name);
  sources_lost->lost_sources[sources_lost->num_lost_sources].terminated = terminated;
  ++sources_lost->num_lost_sources;

  return true;
}

/*
 * Add a new found source to a SourcesFoundNotification.
 *
 * [out] sources_found SourcesFoundNotification instance to which to append the found source.
 * [in] source The source that was found.
 */
bool add_found_source(SourcesFoundNotification* sources_found, const SacnTrackedSource* source)
{
  SACN_ASSERT(sources_found);
  SACN_ASSERT(source);

  CHECK_ROOM_FOR_ONE_MORE(sources_found, found_sources, SacnFoundSource, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);

  sources_found->found_sources[sources_found->num_found_sources].cid = source->cid;
  ETCPAL_MSVC_NO_DEP_WRN strcpy(sources_found->found_sources[sources_found->num_found_sources].name, source->name);
  sources_found->found_sources[sources_found->num_found_sources].from_addr = source->null_start_code_buffer.from_addr;
  sources_found->found_sources[sources_found->num_found_sources].priority = source->null_start_code_buffer.priority;
  sources_found->found_sources[sources_found->num_found_sources].values = source->null_start_code_buffer.data;
  sources_found->found_sources[sources_found->num_found_sources].values_len = source->null_start_code_buffer.slot_count;
  sources_found->found_sources[sources_found->num_found_sources].preview = source->null_start_code_buffer.preview;
  sources_found->found_sources[sources_found->num_found_sources].per_address = source->pap_buffer.data;
  sources_found->found_sources[sources_found->num_found_sources].per_address_len = source->pap_buffer.slot_count;
  ++sources_found->num_found_sources;

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

  CHECK_ROOM_FOR_ONE_MORE(recv_thread_context, dead_sockets, etcpal_socket_t, SACN_RECEIVER_MAX_UNIVERSES * 2);

  recv_thread_context->dead_sockets[recv_thread_context->num_dead_sockets++] = socket;
  return true;
}

#if SACN_RECEIVER_SOCKET_PER_UNIVERSE

/*
 * Add a new dead socket to a SacnRecvThreadContext.
 *
 * [out] recv_thread_context SacnRecvThreadConstext instance to which to append the dead socket.
 * [in] socket Dead socket.
 * Returns true if the socket was successfully added, false if memory could not be allocated.
 */
bool add_pending_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket)
{
  SACN_ASSERT(recv_thread_context);

  CHECK_ROOM_FOR_ONE_MORE(recv_thread_context, pending_sockets, etcpal_socket_t, SACN_RECEIVER_MAX_UNIVERSES * 2);

  recv_thread_context->pending_sockets[recv_thread_context->num_pending_sockets++] = socket;
  return true;
}

#else  // SACN_RECEIVER_SOCKET_PER_UNIVERSE

bool add_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket)
{
  SACN_ASSERT(recv_thread_context);

  CHECK_ROOM_FOR_ONE_MORE(recv_thread_context, socket_refs, SocketRef, SACN_RECEIVER_MAX_SOCKET_REFS);

  recv_thread_context->socket_refs[recv_thread_context->num_socket_refs].sock = socket;
  recv_thread_context->socket_refs[recv_thread_context->num_socket_refs].refcount = 1;
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

#endif  // SACN_RECEIVER_SOCKET_PER_UNIVERSE

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

void zero_sources_found_array(SourcesFoundNotification* sources_found_arr, size_t size)
{
  for (SourcesFoundNotification* sources_found = sources_found_arr; sources_found < sources_found_arr + size; ++sources_found)
  {
    sources_found->callback = NULL;
    sources_found->handle = SACN_RECEIVER_INVALID;
    sources_found->num_found_sources = 0;
    sources_found->context = NULL;
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

#if SACN_RECEIVER_SOCKET_PER_UNIVERSE
  recv_thread_context->pending_sockets = calloc(INITIAL_CAPACITY, sizeof(etcpal_socket_t));
  if (!recv_thread_context->pending_sockets)
    return kEtcPalErrNoMem;
  recv_thread_context->pending_sockets_capacity = INITIAL_CAPACITY;
#else
  recv_thread_context->socket_refs = calloc(INITIAL_CAPACITY, sizeof(SocketRef));
  if (!recv_thread_context->socket_refs)
    return kEtcPalErrNoMem;
  recv_thread_context->socket_refs_capacity = INITIAL_CAPACITY;
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

etcpal_error_t init_sources_found_bufs(unsigned int num_threads)
{
  mem_bufs.sources_found = calloc(num_threads, sizeof(SourcesFoundNotificationBuf));
  if (!mem_bufs.sources_found)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_sources_found_buf(&mem_bufs.sources_found[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_sources_found_buf(SourcesFoundNotificationBuf* sources_found_buf)
{
  SACN_ASSERT(sources_found_buf);

  sources_found_buf->buf = calloc(INITIAL_CAPACITY, sizeof(SourcesFoundNotification));
  if (!sources_found_buf->buf)
    return kEtcPalErrNoMem;

  sources_found_buf->buf_capacity = INITIAL_CAPACITY;
  return init_sources_found_array(sources_found_buf->buf, INITIAL_CAPACITY);
}

etcpal_error_t init_sources_found_array(SourcesFoundNotification* sources_found_arr, size_t size)
{
  SACN_ASSERT(sources_found_arr);

  for (SourcesFoundNotification* sources_found = sources_found_arr; sources_found < sources_found_arr + size; ++sources_found)
  {
    sources_found->found_sources = calloc(INITIAL_CAPACITY, sizeof(SacnFoundSource));
    if (!sources_found->found_sources)
      return kEtcPalErrNoMem;
    sources_found->found_sources_capacity = INITIAL_CAPACITY;
  }
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
#if SACN_RECEIVER_SOCKET_PER_UNIVERSE
  if (recv_thread_context->pending_sockets)
    free(recv_thread_context->pending_sockets);
#else
  if (recv_thread_context->socket_refs)
    free(recv_thread_context->socket_refs);
#endif
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

void deinit_sources_found_bufs(void)
{
  if (mem_bufs.sources_found)
  {
    for (unsigned int i = 0; i < mem_bufs.num_threads; ++i)
      deinit_sources_found_buf(&mem_bufs.sources_found[i]);
    free(mem_bufs.sources_found);
  }
}

void deinit_sources_found_buf(SourcesFoundNotificationBuf* sources_found_buf)
{
  SACN_ASSERT(sources_found_buf);

  if (sources_found_buf->buf)
  {
    for (size_t i = 0; i < sources_found_buf->buf_capacity; ++i)
    {
      deinit_sources_found_entry(&sources_found_buf->buf[i]);
    }
    free(sources_found_buf->buf);
  }
}

void deinit_sources_found_entry(SourcesFoundNotification* sources_found)
{
  SACN_ASSERT(sources_found);

  if (sources_found->found_sources)
    free(sources_found->found_sources);
}

void deinit_source_pap_lost_buf(void)
{
  if (mem_bufs.source_pap_lost)
    free(mem_bufs.source_pap_lost);
}

void deinit_source_limit_exceeded_buf(void)
{
  if (mem_bufs.source_limit_exceeded)
    free(mem_bufs.source_limit_exceeded);
}

#endif  // SACN_DYNAMIC_MEM
