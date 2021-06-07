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
static SacnRecvThreadContext* recv_thread_context;
#else  // SACN_DYNAMIC_MEM
static SacnRecvThreadContext recv_thread_context[SACN_RECEIVER_MAX_THREADS];
#endif  // SACN_DYNAMIC_MEM

/*********************** Private function prototypes *************************/

#if SACN_DYNAMIC_MEM
// Dynamic memory initialization
static etcpal_error_t init_recv_thread_context_entry(SacnRecvThreadContext* context);

// Dynamic memory deinitialization
static void deinit_recv_thread_context_entry(SacnRecvThreadContext* context);
#endif  // SACN_DYNAMIC_MEM

/*************************** Function definitions ****************************/

/*
 * Get a buffer to read incoming sACN data into for a given thread. The buffer will not be
 * initialized.
 *
 * Returns the buffer or NULL if the thread ID was invalid.
 */
SacnRecvThreadContext* get_recv_thread_context(sacn_thread_id_t thread_id)
{
  if (thread_id < sacn_mem_get_num_threads())
  {
    SacnRecvThreadContext* to_return = &recv_thread_context[thread_id];
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
bool add_dead_socket(SacnRecvThreadContext* context, etcpal_socket_t socket)
{
  SACN_ASSERT(context);

  CHECK_ROOM_FOR_ONE_MORE(context, dead_sockets, etcpal_socket_t, SACN_RECEIVER_MAX_UNIVERSES * 2, false);

  context->dead_sockets[context->num_dead_sockets++] = socket;
  return true;
}

bool add_socket_ref(SacnRecvThreadContext* context, etcpal_socket_t socket, etcpal_iptype_t ip_type,
                    bool bound)
{
#if !SACN_RECEIVER_LIMIT_BIND
  ETCPAL_UNUSED_ARG(bound);
#endif

  SACN_ASSERT(context);

  CHECK_ROOM_FOR_ONE_MORE(context, socket_refs, SocketRef, SACN_RECEIVER_MAX_SOCKET_REFS, false);

  context->socket_refs[context->num_socket_refs].sock = socket;
  context->socket_refs[context->num_socket_refs].refcount = 1;
  context->socket_refs[context->num_socket_refs].ip_type = ip_type;
#if SACN_RECEIVER_LIMIT_BIND
  context->socket_refs[context->num_socket_refs].bound = bound;
#endif
  ++context->num_socket_refs;
  ++context->new_socket_refs;
  return true;
}

bool remove_socket_ref(SacnRecvThreadContext* context, etcpal_socket_t socket)
{
  SACN_ASSERT(context);

  for (size_t i = 0; i < context->num_socket_refs; ++i)
  {
    SocketRef* ref = &context->socket_refs[i];
    if (ref->sock == socket)
    {
      if (--ref->refcount == 0)
      {
        if (i < context->num_socket_refs - 1)
          memmove(ref, ref + 1, (context->num_socket_refs - 1 - i) * sizeof(SocketRef));
        --context->num_socket_refs;
        return true;
      }
    }
  }
  return false;
}

void add_receiver_to_list(SacnRecvThreadContext* context, SacnReceiver* receiver)
{
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

#if SACN_DYNAMIC_MEM

etcpal_error_t init_recv_thread_context_buf(unsigned int num_threads)
{
  recv_thread_context = calloc(num_threads, sizeof(SacnRecvThreadContext));
  if (!recv_thread_context)
    return kEtcPalErrNoMem;

  for (unsigned int i = 0; i < num_threads; ++i)
  {
    etcpal_error_t res = init_recv_thread_context_entry(&recv_thread_context[i]);
    if (res != kEtcPalErrOk)
      return res;
  }
  return kEtcPalErrOk;
}

etcpal_error_t init_recv_thread_context_entry(SacnRecvThreadContext* context)
{
  SACN_ASSERT(context);

  context->dead_sockets = calloc(INITIAL_CAPACITY, sizeof(etcpal_socket_t));
  if (!context->dead_sockets)
    return kEtcPalErrNoMem;
  context->dead_sockets_capacity = INITIAL_CAPACITY;

  context->socket_refs = calloc(INITIAL_CAPACITY, sizeof(SocketRef));
  if (!context->socket_refs)
    return kEtcPalErrNoMem;
  context->socket_refs_capacity = INITIAL_CAPACITY;

  context->num_socket_refs = 0;
  context->new_socket_refs = 0;
#if SACN_RECEIVER_LIMIT_BIND
  context->ipv4_bound = false;
  context->ipv6_bound = false;
#endif

  context->source_detector = NULL;

  return kEtcPalErrOk;
}

void deinit_recv_thread_context_buf(void)
{
  if (recv_thread_context)
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
      deinit_recv_thread_context_entry(&recv_thread_context[i]);

    free(recv_thread_context);
    recv_thread_context = NULL;
  }
}

void deinit_recv_thread_context_entry(SacnRecvThreadContext* context)
{
  SACN_ASSERT(context);
  CLEAR_BUF(context, dead_sockets);
  CLEAR_BUF(context, socket_refs);
}

#endif  // SACN_DYNAMIC_MEM

#endif  // SACN_RECEIVER_ENABLED
