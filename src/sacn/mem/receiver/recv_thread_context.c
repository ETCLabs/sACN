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

#include "sacn/private/mem/receiver/recv_thread_context.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_RECEIVER_ENABLED

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static SacnRecvThreadContext* sacn_pool_recv_thread_context;
#else   // SACN_DYNAMIC_MEM
static SacnRecvThreadContext sacn_pool_recv_thread_context[SACN_RECEIVER_MAX_THREADS];
#endif  // SACN_DYNAMIC_MEM

/*********************** Private function prototypes *************************/

// Dynamic memory initialization
static etcpal_error_t init_recv_thread_context_entry(SacnRecvThreadContext* context);

// Dynamic memory deinitialization
static void deinit_recv_thread_context_entry(SacnRecvThreadContext* context);

// Utilities
bool remove_socket_group_req(SocketGroupReq* reqs, size_t* num_reqs, etcpal_socket_t sock, const EtcPalGroupReq* group);

/*************************** Function definitions ****************************/

/*
 * Get a buffer to read incoming sACN data into for a given thread. The buffer will not be
 * initialized.
 *
 * Returns the buffer or NULL if the thread ID was invalid.
 */
SacnRecvThreadContext* get_recv_thread_context(sacn_thread_id_t thread_id)
{
  if (!SACN_ASSERT_VERIFY(thread_id != SACN_THREAD_ID_INVALID))
    return NULL;

  if (thread_id < sacn_mem_get_num_threads())
  {
    SacnRecvThreadContext* to_return = &sacn_pool_recv_thread_context[thread_id];
    to_return->thread_id = thread_id;
    return to_return;
  }
  return NULL;
}

/*
 * Add a new dead socket to a SacnRecvThreadContext.
 *
 * [out] context SacnRecvThreadConstext instance to which to append the dead socket.
 * [in] socket Dead socket.
 * Returns true if the socket was successfully added, false if memory could not be allocated.
 */
bool add_dead_socket(SacnRecvThreadContext* context, const ReceiveSocket* socket)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(socket))
    return false;

  CHECK_ROOM_FOR_ONE_MORE(context, dead_sockets, ReceiveSocket, SACN_RECEIVER_MAX_UNIVERSES * 2, false);

  context->dead_sockets[context->num_dead_sockets++] = *socket;
  return true;
}

// Return index in context->socket_refs or -1 if not enough room.
int add_socket_ref(SacnRecvThreadContext* context, const ReceiveSocket* socket)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(socket))
    return -1;

  CHECK_ROOM_FOR_ONE_MORE(context, socket_refs, SocketRef, SACN_RECEIVER_MAX_SOCKET_REFS, -1);

  int index = (int)context->num_socket_refs;

  context->socket_refs[index].socket = *socket;
  context->socket_refs[index].refcount = 1;
  context->socket_refs[index].pending = true;

  ++context->num_socket_refs;
  ++context->new_socket_refs;

  if (socket->bound)
    mark_socket_ref_bound(context, index);

  return index;
}

/*
 * Add a new subscribe operation to a SacnRecvThreadContext.
 *
 * [out] context SacnRecvThreadConstext instance to which to append the subscribe.
 * [in] sock Socket for which to do the subscribe.
 * [in] group Multicast group and interface to subscribe on.
 * Returns true if the subscribe was successfully added, false if memory could not be allocated.
 */
bool add_subscribe(SacnRecvThreadContext* context, etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) || !SACN_ASSERT_VERIFY(group))
    return false;

  CHECK_ROOM_FOR_ONE_MORE(context, subscribes, SocketGroupReq, (SACN_MAX_NETINTS * SACN_MAX_SUBSCRIPTIONS), false);

  SocketGroupReq subscribe;
  subscribe.socket = sock;
  subscribe.group = *group;
  context->subscribes[context->num_subscribes++] = subscribe;
  return true;
}

/*
 * Add a new unsubscribe operation to a SacnRecvThreadContext.
 *
 * [out] context SacnRecvThreadConstext instance to which to append the unsubscribe.
 * [in] sock Socket for which to do the unsubscribe.
 * [in] group Multicast group and interface to unsubscribe from.
 * Returns true if the unsubscribe was successfully added, false if memory could not be allocated.
 */
bool add_unsubscribe(SacnRecvThreadContext* context, etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) || !SACN_ASSERT_VERIFY(group))
    return false;

  CHECK_ROOM_FOR_ONE_MORE(context, unsubscribes, SocketGroupReq, (SACN_MAX_NETINTS * SACN_MAX_SUBSCRIPTIONS), false);

  SocketGroupReq unsubscribe;
  unsubscribe.socket = sock;
  unsubscribe.group = *group;
  context->unsubscribes[context->num_unsubscribes++] = unsubscribe;
  return true;
}

// Return index in context->socket_refs or -1 if not found.
#if SACN_RECEIVER_SOCKET_PER_NIC
int find_socket_ref_with_room(SacnRecvThreadContext* context, etcpal_iptype_t ip_type, unsigned int ifindex)
#else
int find_socket_ref_with_room(SacnRecvThreadContext* context, etcpal_iptype_t ip_type)
#endif
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(ip_type != kEtcPalIpTypeInvalid))
    return -1;

  int index = 0;
  for (SocketRef* entry = context->socket_refs; entry < (context->socket_refs + context->num_socket_refs); ++entry)
  {
    bool matches = (entry->socket.ip_type == ip_type);
#if SACN_RECEIVER_SOCKET_PER_NIC
    matches = matches && (entry->socket.ifindex == ifindex);
#endif

    if (matches && (entry->refcount < SACN_RECEIVER_MAX_SUBS_PER_SOCKET))
      return index;
    else
      ++index;
  }

  return -1;
}

// Return index in context->socket_refs or -1 if not found.
int find_socket_ref_by_type(SacnRecvThreadContext* context, etcpal_iptype_t ip_type)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(ip_type != kEtcPalIpTypeInvalid))
    return -1;

  int index = 0;
  for (SocketRef* entry = context->socket_refs; entry < (context->socket_refs + context->num_socket_refs); ++entry)
  {
    if (entry->socket.ip_type == ip_type)
      return index;
    else
      ++index;
  }

  return -1;
}

// Return index in context->socket_refs or -1 if not found.
int find_socket_ref_by_handle(SacnRecvThreadContext* context, etcpal_socket_t handle)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(handle != ETCPAL_SOCKET_INVALID))
    return -1;

  int index = 0;
  for (SocketRef* entry = context->socket_refs; entry < (context->socket_refs + context->num_socket_refs); ++entry)
  {
    if (entry->socket.handle == handle)
      return index;
    else
      ++index;
  }

  return -1;
}

void mark_socket_ref_bound(SacnRecvThreadContext* context, int index)
{
  if (!SACN_ASSERT_VERIFY(context))
    return;

  SocketRef* ref = &context->socket_refs[index];
  ref->socket.bound = true;

#if SACN_RECEIVER_LIMIT_BIND
  if (ref->socket.ip_type == kEtcPalIpTypeV4)
    context->ipv4_bound = true;
  else if (ref->socket.ip_type == kEtcPalIpTypeV6)
    context->ipv6_bound = true;
#endif
}

bool remove_socket_ref(SacnRecvThreadContext* context, int index)
{
  if (!SACN_ASSERT_VERIFY(context))
    return false;

  SocketRef* ref = &context->socket_refs[index];
  if (--ref->refcount == 0)
  {
#if SACN_RECEIVER_LIMIT_BIND
    etcpal_iptype_t ip_type = ref->socket.ip_type;
    bool was_bound = ref->socket.bound;
#endif
    bool was_pending = ref->pending;

    if (index < (context->num_socket_refs - 1))
      memmove(ref, ref + 1, (context->num_socket_refs - 1 - index) * sizeof(SocketRef));

    --context->num_socket_refs;
    if (was_pending)
      --context->new_socket_refs;
#if SACN_RECEIVER_LIMIT_BIND
    if (was_bound)
    {
      if (ip_type == kEtcPalIpTypeV4)
        context->ipv4_bound = false;
      else if (ip_type == kEtcPalIpTypeV6)
        context->ipv6_bound = false;
    }
#endif

    return true;
  }
  return false;
}

bool remove_subscribe(SacnRecvThreadContext* context, etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) || !SACN_ASSERT_VERIFY(group))
    return false;

  return remove_socket_group_req(context->subscribes, &context->num_subscribes, sock, group);
}

bool remove_unsubscribe(SacnRecvThreadContext* context, etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) || !SACN_ASSERT_VERIFY(group))
    return false;

  return remove_socket_group_req(context->unsubscribes, &context->num_unsubscribes, sock, group);
}

void add_receiver_to_list(SacnRecvThreadContext* context, SacnReceiver* receiver)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(receiver))
    return;

  SacnReceiver* list_entry = context->receivers;
  while (list_entry && list_entry->next)
    list_entry = list_entry->next;

  if (list_entry)
    list_entry->next = receiver;
  else
    context->receivers = receiver;

  ++context->num_receivers;
}

void remove_receiver_from_list(SacnRecvThreadContext* context, SacnReceiver* receiver)
{
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(receiver))
    return;

  SacnReceiver* last = NULL;
  SacnReceiver* entry = context->receivers;
  while (entry)
  {
    if (entry == receiver)
    {
      if (!last)
      {
        // replace the head
        context->receivers = entry->next;
      }
      else
      {
        last->next = entry->next;
      }

      if (context->num_receivers > 0)
        --context->num_receivers;

      receiver->next = NULL;
      break;
    }
    last = entry;
    entry = entry->next;
  }
}

etcpal_error_t init_recv_thread_context_buf(unsigned int num_threads)
{
  if (!SACN_ASSERT_VERIFY(num_threads > 0))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  sacn_pool_recv_thread_context = calloc(num_threads, sizeof(SacnRecvThreadContext));
  if (!sacn_pool_recv_thread_context)
    return kEtcPalErrNoMem;
#else   // SACN_DYNAMIC_MEM
  memset(sacn_pool_recv_thread_context, 0, sizeof(sacn_pool_recv_thread_context));
  if (num_threads > SACN_RECEIVER_MAX_THREADS)
    return kEtcPalErrNoMem;
#endif  // SACN_DYNAMIC_MEM

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_recv_thread_context_entry(&sacn_pool_recv_thread_context[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_recv_thread_context_entry(SacnRecvThreadContext* context)
{
  if (!SACN_ASSERT_VERIFY(context))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  context->dead_sockets = calloc(INITIAL_CAPACITY, sizeof(ReceiveSocket));
  if (!context->dead_sockets)
    return kEtcPalErrNoMem;
  context->dead_sockets_capacity = INITIAL_CAPACITY;

  context->socket_refs = calloc(INITIAL_CAPACITY, sizeof(SocketRef));
  if (!context->socket_refs)
    return kEtcPalErrNoMem;
  context->socket_refs_capacity = INITIAL_CAPACITY;

  context->subscribes = calloc(INITIAL_CAPACITY, sizeof(SocketGroupReq));
  if (!context->subscribes)
    return kEtcPalErrNoMem;
  context->subscribes_capacity = INITIAL_CAPACITY;

  context->unsubscribes = calloc(INITIAL_CAPACITY, sizeof(SocketGroupReq));
  if (!context->unsubscribes)
    return kEtcPalErrNoMem;
  context->unsubscribes_capacity = INITIAL_CAPACITY;
#endif

  context->num_dead_sockets = 0;
  context->num_socket_refs = 0;
  context->new_socket_refs = 0;
  context->num_subscribes = 0;
  context->num_unsubscribes = 0;

#if SACN_RECEIVER_LIMIT_BIND
  context->ipv4_bound = false;
  context->ipv6_bound = false;
#endif

  context->source_detector = NULL;

  context->running = false;
  context->poll_context_initialized = false;
  context->periodic_timer_started = false;

  return kEtcPalErrOk;
}

void deinit_recv_thread_context_buf(void)
{
#if SACN_DYNAMIC_MEM
  if (sacn_pool_recv_thread_context)
#endif
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_recv_thread_context_entry(&sacn_pool_recv_thread_context[i]);

#if SACN_DYNAMIC_MEM
    free(sacn_pool_recv_thread_context);
    sacn_pool_recv_thread_context = NULL;
#endif
  }
}

void deinit_recv_thread_context_entry(SacnRecvThreadContext* context)
{
  if (!SACN_ASSERT_VERIFY(context))
    return;

  CLEAR_BUF(context, dead_sockets);
  CLEAR_BUF(context, socket_refs);
  CLEAR_BUF(context, subscribes);
  CLEAR_BUF(context, unsubscribes);
}

bool remove_socket_group_req(SocketGroupReq* reqs, size_t* num_reqs, etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  if (!SACN_ASSERT_VERIFY(reqs) || !SACN_ASSERT_VERIFY(num_reqs) || !SACN_ASSERT_VERIFY(group))
    return false;

  size_t index = 0;
  SocketGroupReq* req = NULL;
  bool found = false;
  for (size_t i = 0; !found && (i < *num_reqs); ++i)
  {
    req = &reqs[i];
    if ((req->socket == sock) && (req->group.ifindex == group->ifindex) &&
        (etcpal_ip_cmp(&req->group.group, &group->group) == 0))
    {
      index = i;
      found = true;
    }
  }

  if (found)
  {
    if (index < (*num_reqs - 1))
      memmove(req, req + 1, (*num_reqs - 1 - index) * sizeof(SocketGroupReq));

    --(*num_reqs);
  }

  return found;
}

#endif  // SACN_RECEIVER_ENABLED
