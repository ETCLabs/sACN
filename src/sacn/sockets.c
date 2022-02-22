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

#include "sacn/private/sockets.h"

#include "etcpal/common.h"
#include "etcpal/netint.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static etcpal_socket_t* multicast_send_sockets;
#else
static etcpal_socket_t multicast_send_sockets[SACN_MAX_NETINTS];
#endif
static SacnSocketsSysNetints receiver_sys_netints;
static SacnSocketsSysNetints source_detector_sys_netints;
static SacnSocketsSysNetints source_sys_netints;
static etcpal_socket_t ipv4_unicast_send_socket;
static etcpal_socket_t ipv6_unicast_send_socket;

/*********************** Private function prototypes *************************/

static etcpal_error_t sockets_init(const SacnNetintConfig* netint_config, networking_type_t net_type);
static etcpal_error_t sockets_reset(const SacnNetintConfig* netint_config, networking_type_t net_type);
static void clear_source_networking();
static etcpal_error_t validate_netint_config(const SacnNetintConfig* netint_config,
                                             const SacnMcastInterface* sys_netints, size_t num_sys_netints,
                                             size_t* num_valid_netints);
static bool netints_valid(const SacnMcastInterface* netints, size_t num_netints);
static size_t apply_netint_config(const SacnNetintConfig* netint_config, size_t num_netints_to_apply,
                                  SacnSocketsSysNetints* sys_netints, networking_type_t net_type);
static etcpal_error_t test_netints(const EtcPalNetintInfo* netints, size_t num_netints, sacn_ip_support_t ip_type,
                                   SacnSocketsSysNetints* sys_netints, networking_type_t net_type,
                                   size_t* num_valid_sys_netints);
static etcpal_error_t test_netint(const EtcPalNetintInfo* netint, SacnSocketsSysNetints* sys_netints,
                                  networking_type_t net_type);
static etcpal_error_t test_sacn_receiver_netint(unsigned int index, etcpal_iptype_t ip_type, const EtcPalIpAddr* addr,
                                                SacnMcastInterface* sys_netints, size_t* num_sys_netints);
static etcpal_error_t test_sacn_source_netint(unsigned int index, etcpal_iptype_t ip_type, const EtcPalIpAddr* addr);
static etcpal_error_t init_unicast_send_sockets();
static bool add_sacn_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status,
                                SacnMcastInterface* sys_netints, size_t* num_sys_netints);
static void add_sacn_source_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status);
static int netint_id_index_in_array(const EtcPalMcastNetintId* id, const SacnMcastInterface* array, size_t array_size);

static etcpal_error_t create_multicast_send_socket(const EtcPalMcastNetintId* netint_id, etcpal_socket_t* socket);
static etcpal_error_t create_unicast_send_socket(etcpal_iptype_t ip_type, etcpal_socket_t* socket);
static etcpal_error_t create_receive_socket(etcpal_iptype_t ip_type, const EtcPalSockAddr* bind_addr, bool set_sockopts,
                                            ReceiveSocket* socket);
static void poll_add_socket(SacnRecvThreadContext* recv_thread_context, ReceiveSocket* socket);
#if SACN_RECEIVER_ENABLED
static etcpal_error_t queue_subscription(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t sock,
                                         const EtcPalIpAddr* group, const EtcPalMcastNetintId* netints,
                                         size_t num_netints);
static etcpal_error_t unsubscribe_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t sock,
                                         const EtcPalIpAddr* group, const EtcPalMcastNetintId* netints,
                                         size_t num_netints, socket_cleanup_behavior_t cleanup_behavior);
static void unsubscribe_socket_ref(SacnRecvThreadContext* recv_thread_context, int ref_index, uint16_t universe,
                                   const EtcPalMcastNetintId* netints, size_t num_netints,
                                   socket_cleanup_behavior_t cleanup_behavior);
static void cleanup_receive_socket(SacnRecvThreadContext* context, const ReceiveSocket* socket,
                                   socket_cleanup_behavior_t cleanup_behavior);
#endif  // SACN_RECEIVER_ENABLED
static etcpal_error_t subscribe_on_single_interface(etcpal_socket_t sock, const EtcPalGroupReq* group);
static etcpal_error_t unsubscribe_on_single_interface(etcpal_socket_t sock, const EtcPalGroupReq* group);
static void send_multicast(uint16_t universe_id, etcpal_iptype_t ip_type, const uint8_t* send_buf,
                           const EtcPalMcastNetintId* netint);
static void send_unicast(const uint8_t* send_buf, const EtcPalIpAddr* dest_addr);
#if SACN_RECEIVER_ENABLED
static EtcPalSockAddr get_bind_address(etcpal_iptype_t ip_type);
#endif  // SACN_RECEIVER_ENABLED

/*************************** Function definitions ****************************/

etcpal_error_t sacn_sockets_init(const SacnNetintConfig* netint_config)
{
  etcpal_error_t res = (netint_config && !netints_valid(netint_config->netints, netint_config->num_netints))
                           ? kEtcPalErrInvalid
                           : kEtcPalErrOk;
  if (res == kEtcPalErrOk)
    res = sockets_init(netint_config, kSource);
  if (res == kEtcPalErrOk)
    res = sockets_init(netint_config, kReceiver);
  if (res == kEtcPalErrOk)
    res = sockets_init(netint_config, kSourceDetector);

  if (res != kEtcPalErrOk)
  {
    clear_source_networking();

    CLEAR_BUF(&receiver_sys_netints, sys_netints);
    CLEAR_BUF(&source_detector_sys_netints, sys_netints);
  }

  return res;
}

void sacn_sockets_deinit(void)
{
  clear_source_networking();

  CLEAR_BUF(&receiver_sys_netints, sys_netints);
  CLEAR_BUF(&source_detector_sys_netints, sys_netints);
}

etcpal_error_t sacn_sockets_reset_source(const SacnNetintConfig* netint_config)
{
  return sockets_reset(netint_config, kSource);
}

etcpal_error_t sacn_sockets_reset_receiver(const SacnNetintConfig* netint_config)
{
  return sockets_reset(netint_config, kReceiver);
}

etcpal_error_t sacn_sockets_reset_source_detector(const SacnNetintConfig* netint_config)
{
  return sockets_reset(netint_config, kSourceDetector);
}

#if SACN_RECEIVER_ENABLED

void unsubscribe_socket_ref(SacnRecvThreadContext* recv_thread_context, int ref_index, uint16_t universe,
                            const EtcPalMcastNetintId* netints, size_t num_netints,
                            socket_cleanup_behavior_t cleanup_behavior)
{
  ReceiveSocket socket = recv_thread_context->socket_refs[ref_index].socket;

  EtcPalIpAddr group;
  sacn_get_mcast_addr(socket.ip_type, universe, &group);

  unsubscribe_socket(recv_thread_context, socket.handle, &group, netints, num_netints, cleanup_behavior);
  if (remove_socket_ref(recv_thread_context, ref_index))
    cleanup_receive_socket(recv_thread_context, &socket, cleanup_behavior);
}

void cleanup_receive_socket(SacnRecvThreadContext* context, const ReceiveSocket* socket,
                            socket_cleanup_behavior_t cleanup_behavior)
{
  switch (cleanup_behavior)
  {
    case kPerformAllSocketCleanupNow:
      if (context->poll_context_initialized && socket->polling)
        etcpal_poll_remove_socket(&context->poll_context, socket->handle);

      etcpal_close(socket->handle);

#if SACN_RECEIVER_LIMIT_BIND
      // The socket has already been removed from the SocketRef array, so the context's bound flags are up-to-date.
      // Check the bound flags to see if a new SocketRef hasn't already been bound (possible if this was queued).
      if (socket->bound && (((socket->ip_type == kEtcPalIpTypeV4) && (!context->ipv4_bound)) ||
                            ((socket->ip_type == kEtcPalIpTypeV6) && (!context->ipv6_bound))))
      {
        // At least one socket (if there are any) needs to be bound, so find a new "successor" socket to bind.
        int successor_index = find_socket_ref_by_type(context, socket->ip_type);
        if (successor_index >= 0)
        {
          EtcPalSockAddr recv_any = get_bind_address(socket->ip_type);
          SocketRef* successor = &context->socket_refs[successor_index];
          if (etcpal_bind(successor->socket.handle, &recv_any) == kEtcPalErrOk)
          {
            mark_socket_ref_bound(context, successor_index);

            if (!successor->pending)
              poll_add_socket(context, &successor->socket);
          }
        }
      }
#endif  // SACN_RECEIVER_LIMIT_BIND
      break;
    case kQueueSocketCleanup:
      // We don't clean up the socket here, due to potential thread safety issues.
      // It gets added to a queue, where eventually the socket read thread calls this with kPerformAllSocketCleanupNow.
      add_dead_socket(context, socket);
      break;
  }
}

#endif  // SACN_RECEIVER_ENABLED

void send_multicast(uint16_t universe_id, etcpal_iptype_t ip_type, const uint8_t* send_buf,
                    const EtcPalMcastNetintId* netint)
{
  // Determine the multicast destination
  EtcPalSockAddr dest;
  sacn_get_mcast_addr(ip_type, universe_id, &dest.ip);
  dest.port = SACN_PORT;

  // Determine the socket to use
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  int sys_netint_index =
      netint_id_index_in_array(netint, source_sys_netints.sys_netints, source_sys_netints.num_sys_netints);
  if ((sys_netint_index >= 0) && (sys_netint_index < (int)source_sys_netints.num_sys_netints))
    sock = multicast_send_sockets[sys_netint_index];

  // Try to send the data (ignore errors)
  const size_t send_buf_length =
      (size_t)ACN_UDP_PREAMBLE_SIZE + (size_t)ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE]));

  if (sock != ETCPAL_SOCKET_INVALID)
    etcpal_sendto(sock, send_buf, send_buf_length, 0, &dest);
}

void send_unicast(const uint8_t* send_buf, const EtcPalIpAddr* dest_addr)
{
  // Determine the socket to use
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  if (dest_addr->type == kEtcPalIpTypeV4)
    sock = ipv4_unicast_send_socket;
  else if (dest_addr->type == kEtcPalIpTypeV6)
    sock = ipv6_unicast_send_socket;

  if (sock != ETCPAL_SOCKET_INVALID)
  {
    // Convert destination to SockAddr
    EtcPalSockAddr sockaddr_dest;
    sockaddr_dest.ip = *dest_addr;
    sockaddr_dest.port = SACN_PORT;

    // Try to send the data (ignore errors)
    const size_t send_buf_length =
        (size_t)ACN_UDP_PREAMBLE_SIZE + (size_t)ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE]));

    etcpal_sendto(sock, send_buf, send_buf_length, 0, &sockaddr_dest);
  }
}

#if SACN_RECEIVER_ENABLED

EtcPalSockAddr get_bind_address(etcpal_iptype_t ip_type)
{
  EtcPalSockAddr recv_any;
  etcpal_ip_set_wildcard(ip_type, &recv_any.ip);
  recv_any.port = SACN_PORT;
  return recv_any;
}

#endif  // SACN_RECEIVER_ENABLED

/*
 * Internal function to create a new send socket for multicast, associated with an interface.
 * There is a one-to-one relationship between interfaces and multicast send sockets.
 *
 * [in] netint_id Network interface identifier.
 * [out] new_sock Filled in with new socket descriptor.
 * Returns kEtcPalErrOk (success) or a relevant error code on failure.
 */
etcpal_error_t create_multicast_send_socket(const EtcPalMcastNetintId* netint_id, etcpal_socket_t* socket)
{
  int sockopt_ip_level = (netint_id->ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP);

  etcpal_socket_t new_sock = ETCPAL_SOCKET_INVALID;
  etcpal_error_t res = etcpal_socket(netint_id->ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET,
                                     ETCPAL_SOCK_DGRAM, &new_sock);

  if (res == kEtcPalErrOk)
  {
    const int value = SACN_SOURCE_MULTICAST_TTL;
    res = etcpal_setsockopt(new_sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_TTL, &value, sizeof value);
  }

  if (res == kEtcPalErrOk)
  {
    res = etcpal_setsockopt(new_sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_IF, &netint_id->index,
                            sizeof netint_id->index);
  }

  if (res == kEtcPalErrOk)
  {
#if SACN_LOOPBACK
    int intval = 1;
    etcpal_setsockopt(new_sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_LOOP, &intval, sizeof intval);
#endif

    *socket = new_sock;
  }
  else if (new_sock != ETCPAL_SOCKET_INVALID)
  {
    etcpal_close(new_sock);
  }

  return res;
}

/*
 * Internal function to create a new send socket for unicast.
 *
 * [in] ip_type The type of IP addresses this socket will send to.
 * [out] new_sock Filled in with new socket descriptor.
 * Returns kEtcPalErrOk (success) or a relevant error code on failure.
 */
etcpal_error_t create_unicast_send_socket(etcpal_iptype_t ip_type, etcpal_socket_t* socket)
{
  return etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, socket);
}

etcpal_error_t create_receive_socket(etcpal_iptype_t ip_type, const EtcPalSockAddr* bind_addr, bool set_sockopts,
                                     ReceiveSocket* socket)
{
  etcpal_socket_t new_sock;
  etcpal_error_t res =
      etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, &new_sock);
  if (res != kEtcPalErrOk)
    return res;

  if (set_sockopts)
  {
    // Set some socket options. We don't check failure on these because they might not work on all
    // platforms.
    int intval = 1;
    etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEADDR, &intval, sizeof intval);
    etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEPORT, &intval, sizeof intval);
    intval = SACN_RECEIVER_SOCKET_RCVBUF_SIZE;
    etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_RCVBUF, &intval, sizeof intval);
  }

  if (bind_addr)
  {
    res = etcpal_bind(new_sock, bind_addr);
    if (res != kEtcPalErrOk)
    {
      etcpal_close(new_sock);
      return res;
    }
  }

  socket->handle = new_sock;
  socket->ip_type = ip_type;
  socket->bound = (bind_addr != NULL);
  socket->polling = false;

  return res;
}

void poll_add_socket(SacnRecvThreadContext* recv_thread_context, ReceiveSocket* socket)
{
  etcpal_error_t add_res = kEtcPalErrNotInit;
  if (recv_thread_context->poll_context_initialized)
    add_res = etcpal_poll_add_socket(&recv_thread_context->poll_context, socket->handle, ETCPAL_POLL_IN, NULL);

  if (add_res == kEtcPalErrOk)
  {
    socket->polling = true;
  }
  else
  {
    SACN_LOG_ERR("Error adding new socket to sACN poll context: '%s'. sACN Receiver will likely not work correctly.",
                 etcpal_strerror(add_res));
  }
}

/*
 * Obtains the sACN multicast address for the given universe and IP type.
 *
 * [in] ip_type Whether the multicast address is IPv4 or IPv6.
 * [in] universe The multicast address' universe.
 * [out] ip The multicast address for the given universe and IP type.
 */
void sacn_get_mcast_addr(etcpal_iptype_t ip_type, uint16_t universe, EtcPalIpAddr* ip)
{
  if (ip_type == kEtcPalIpTypeV4)
  {
    ETCPAL_IP_SET_V4_ADDRESS(ip, (0xefff0000 | universe));
  }
  else
  {
    static const uint8_t ipv6_addr_template[ETCPAL_IPV6_BYTES] = {0xff, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                  0x00, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00};

    ETCPAL_IP_SET_V6_ADDRESS(ip, ipv6_addr_template);
    ip->addr.v6.addr_buf[14] = (uint8_t)(universe >> 8);
    ip->addr.v6.addr_buf[15] = (uint8_t)(universe & 0xff);
  }
}

#if SACN_RECEIVER_ENABLED

/*
 * Creates and subscribes a socket for the given universe.
 *
 * [in,out] uni Universe for which to create and subscribe sockets.
 * Returns kEtcPalErrOk on success, or error code.
 */
etcpal_error_t sacn_add_receiver_socket(sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t universe,
                                        const EtcPalMcastNetintId* netints, size_t num_netints, etcpal_socket_t* socket)
{
  SACN_ASSERT(ip_type == kEtcPalIpTypeV4 || ip_type == kEtcPalIpTypeV6);
  SACN_ASSERT(universe >= 1 && ((universe <= 63999) || (universe == SACN_DISCOVERY_UNIVERSE)));

  SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
  SACN_ASSERT(context);

  etcpal_error_t res = kEtcPalErrOk;

  EtcPalSockAddr universe_mcast_addr;
  sacn_get_mcast_addr(ip_type, universe, &universe_mcast_addr.ip);
  universe_mcast_addr.port = SACN_PORT;

  // Find or create a shared socket.
  SocketRef* ref = NULL;
  int ref_index = find_socket_ref_with_room(context, ip_type);
  if (ref_index >= 0)
  {
    ref = &context->socket_refs[ref_index];
    ++ref->refcount;
  }
  else  // No shared sockets have room; must create a new one.
  {
    EtcPalSockAddr recv_any = get_bind_address(ip_type);
#if SACN_RECEIVER_LIMIT_BIND
    // Limit IPv4 to one bind and IPv6 to one bind for this thread.
    bool perform_bind = (((ip_type == kEtcPalIpTypeV4) && !context->ipv4_bound) ||
                         ((ip_type == kEtcPalIpTypeV6) && !context->ipv6_bound));
#else
    bool perform_bind = true;
#endif
    ReceiveSocket new_socket = RECV_SOCKET_DEFAULT_INIT;
    res = create_receive_socket(ip_type, (perform_bind ? &recv_any : NULL), true, &new_socket);

    // Try to add the new socket ref to the array
    if (res == kEtcPalErrOk)
    {
      ref_index = add_socket_ref(context, &new_socket);

      if (ref_index == -1)
      {
        SACN_LOG_WARNING("Couldn't allocate memory for new sACN receiver socket!");

        res = kEtcPalErrNoMem;

        // Don't need cleanup_receive_socket here - etcpal_close is sufficient.
        etcpal_close(new_socket.handle);
      }
      else
      {
        ref = &context->socket_refs[ref_index];
      }
    }
  }

  if (res == kEtcPalErrOk)
    res = queue_subscription(context, ref->socket.handle, &universe_mcast_addr.ip, netints, num_netints);

  if (res == kEtcPalErrOk)
  {
    *socket = ref->socket.handle;
  }
  else
  {
    if (ref_index >= 0)
      unsubscribe_socket_ref(context, ref_index, universe, netints, num_netints, kQueueSocketCleanup);

    SACN_LOG_WARNING("Couldn't create new sACN receiver socket: '%s'", etcpal_strerror(res));
  }

  return res;
}

void sacn_remove_receiver_socket(sacn_thread_id_t thread_id, etcpal_socket_t* socket, uint16_t universe,
                                 const EtcPalMcastNetintId* netints, size_t num_netints,
                                 socket_cleanup_behavior_t cleanup_behavior)
{
  SACN_ASSERT(socket != NULL);
  SACN_ASSERT(*socket != ETCPAL_SOCKET_INVALID);

  SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
  SACN_ASSERT(context);

  int index = find_socket_ref_by_handle(context, *socket);
  SACN_ASSERT(index >= 0);

  unsubscribe_socket_ref(context, index, universe, netints, num_netints, cleanup_behavior);

  *socket = ETCPAL_SOCKET_INVALID;
}

/*
 * Queues a socket for subscription to a multicast address on all specified network interfaces.
 *
 * [in] recv_thread_context The receiver's thread's context data.
 * [in] sock Socket for which to do the subscribes.
 * [in] group Multicast group address to subscribe the socket to.
 * [in] netints Array of network interfaces to subscribe on. Must not be NULL.
 * [in] num_netints Number of entries in netints array. Must be greater than 0.
 * Returns kEtcPalErrOk if the subscribe was queued successfully, error code otherwise.
 */
etcpal_error_t queue_subscription(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t sock,
                                  const EtcPalIpAddr* group, const EtcPalMcastNetintId* netints, size_t num_netints)
{
  SACN_ASSERT(sock != ETCPAL_SOCKET_INVALID);
  SACN_ASSERT(group);
  SACN_ASSERT(netints);
  SACN_ASSERT(num_netints > 0);

  etcpal_error_t res = kEtcPalErrNoNetints;

  EtcPalGroupReq greq;
  greq.group = *group;

  for (const EtcPalMcastNetintId* netint = netints; netint < netints + num_netints; ++netint)
  {
    if (netint->ip_type == group->type)
    {
      greq.ifindex = netint->index;

      if (remove_unsubscribe(recv_thread_context, sock, &greq))
        res = kEtcPalErrOk;  // Cancelling a previously queued unsub means no sub is needed.
      else
        res = add_subscribe(recv_thread_context, sock, &greq) ? kEtcPalErrOk : kEtcPalErrNoMem;

      if (res != kEtcPalErrOk)
        break;
    }
  }

  return res;
}

/*
 * Unsubscribe (or queue unsubscription of) a socket from a multicast address on all specified network interfaces.
 *
 * [in] recv_thread_context The receiver's thread's context data.
 * [in] sock Socket for which to do the unsubscribes.
 * [in] group Multicast group address to unsubscribe the socket from.
 * [in] netints Array of network interfaces to unsubscribe from. Must not be NULL.
 * [in] num_netints Number of entries in netints array. Must be greater than 0.
 * [in] cleanup_behavior Determines if socket cleanup is being queued or not. If it is, the unsubscribe is also queued.
 * Returns kEtcPalErrOk if the unsubscribe was performed or queued successfully, error code otherwise.
 */
etcpal_error_t unsubscribe_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t sock,
                                  const EtcPalIpAddr* group, const EtcPalMcastNetintId* netints, size_t num_netints,
                                  socket_cleanup_behavior_t cleanup_behavior)
{
  SACN_ASSERT(sock != ETCPAL_SOCKET_INVALID);
  SACN_ASSERT(group);
  SACN_ASSERT(netints);
  SACN_ASSERT(num_netints > 0);

  etcpal_error_t res = kEtcPalErrNoNetints;

  EtcPalGroupReq greq;
  greq.group = *group;

  for (const EtcPalMcastNetintId* netint = netints; netint < netints + num_netints; ++netint)
  {
    if (netint->ip_type == group->type)
    {
      greq.ifindex = netint->index;

      if (remove_subscribe(recv_thread_context, sock, &greq))
        res = kEtcPalErrOk;  // Cancelling a previously queued sub means no unsub is needed.
      else if (cleanup_behavior == kQueueSocketCleanup)
        res = add_unsubscribe(recv_thread_context, sock, &greq) ? kEtcPalErrOk : kEtcPalErrNoMem;
      else if (cleanup_behavior == kPerformAllSocketCleanupNow)
        res = unsubscribe_on_single_interface(sock, &greq);
      else
        res = kEtcPalErrSys;

      if (res != kEtcPalErrOk)
        break;
    }
  }

  return res;
}

#endif  // SACN_RECEIVER_ENABLED

/*
 * Subscribes a socket to a multicast address on a single interface. Logs the failure if the
 * subscribe fails.
 *
 * [in] sock Socket for which to do the subscribe.
 * [in] group Multicast group and interface to subscribe on.
 * Returns kEtcPalErrOk on success, error code on failure.
 */
etcpal_error_t subscribe_on_single_interface(etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  etcpal_error_t res =
      etcpal_setsockopt(sock, group->group.type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                        ETCPAL_MCAST_JOIN_GROUP, group, sizeof(EtcPalGroupReq));
  if (res != kEtcPalErrOk)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char addr_str[ETCPAL_IP_STRING_BYTES];
      etcpal_ip_to_string(&group->group, addr_str);
      SACN_LOG_WARNING("Error subscribing to multicast address %s on network interface index %u: '%s'", addr_str,
                       group->ifindex, etcpal_strerror(res));
    }
  }
  return res;
}

/*
 * Unsubscribes a socket from a multicast address on a single interface. Logs the failure if the
 * unsubscribe fails.
 *
 * [in] sock Socket for which to do the unsubscribe.
 * [in] group Multicast group and interface to unsubscribe on.
 * Returns kEtcPalErrOk on success, error code on failure.
 */
etcpal_error_t unsubscribe_on_single_interface(etcpal_socket_t sock, const EtcPalGroupReq* group)
{
  etcpal_error_t res =
      etcpal_setsockopt(sock, group->group.type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                        ETCPAL_MCAST_LEAVE_GROUP, group, sizeof(EtcPalGroupReq));
  if (res != kEtcPalErrOk)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char addr_str[ETCPAL_IP_STRING_BYTES];
      etcpal_ip_to_string(&group->group, addr_str);
      SACN_LOG_WARNING("Error unsubscribing from multicast address %s on network interface index %u: '%s'", addr_str,
                       group->ifindex, etcpal_strerror(res));
    }
  }
  return res;
}

void sacn_add_pending_sockets(SacnRecvThreadContext* recv_thread_context)
{
  if (recv_thread_context->new_socket_refs)
  {
    for (size_t i = recv_thread_context->num_socket_refs - recv_thread_context->new_socket_refs;
         i < recv_thread_context->num_socket_refs; ++i)
    {
      SocketRef* ref = &recv_thread_context->socket_refs[i];

      if (ref->socket.bound)
        poll_add_socket(recv_thread_context, &ref->socket);

      ref->pending = false;
    }
  }
  recv_thread_context->new_socket_refs = 0;
}

#if SACN_RECEIVER_ENABLED

void sacn_cleanup_dead_sockets(SacnRecvThreadContext* recv_thread_context)
{
  for (const ReceiveSocket* socket = recv_thread_context->dead_sockets;
       socket < recv_thread_context->dead_sockets + recv_thread_context->num_dead_sockets; ++socket)
  {
    cleanup_receive_socket(recv_thread_context, socket, kPerformAllSocketCleanupNow);
  }
  recv_thread_context->num_dead_sockets = 0;
}

#endif  // SACN_RECEIVER_ENABLED

void sacn_subscribe_sockets(SacnRecvThreadContext* recv_thread_context)
{
  for (const SocketGroupReq* req = recv_thread_context->subscribes;
       req < recv_thread_context->subscribes + recv_thread_context->num_subscribes; ++req)
  {
    subscribe_on_single_interface(req->socket, &req->group);
  }
  recv_thread_context->num_subscribes = 0;
}

void sacn_unsubscribe_sockets(SacnRecvThreadContext* recv_thread_context)
{
  for (const SocketGroupReq* req = recv_thread_context->unsubscribes;
       req < recv_thread_context->unsubscribes + recv_thread_context->num_unsubscribes; ++req)
  {
    unsubscribe_on_single_interface(req->socket, &req->group);
  }
  recv_thread_context->num_unsubscribes = 0;
}

/*
 * Read and process input data for a thread's sockets.
 *
 * Blocks up to SACN_RECEIVER_READ_TIMEOUT_MS waiting for data.
 *
 * [in,out] recv_thread_context Context representing the thread calling this function.
 * [out] read_result Filled in with the data if the read was successful on a socket.
 *
 * Returns kEtcPalErrOk if the data has been received.
 * Returns kEtcPalErrTimedOut if the function timed out while waiting for data.
 * Returns other error codes on error. In this case, calling code should sleep to prevent the
 * execution thread from spinning constantly when, for example, there are no receivers listening.
 */
etcpal_error_t sacn_read(SacnRecvThreadContext* recv_thread_context, SacnReadResult* read_result)
{
  SACN_ASSERT(recv_thread_context);
  SACN_ASSERT(read_result);

  EtcPalPollEvent event;
  etcpal_error_t poll_res = etcpal_poll_wait(&recv_thread_context->poll_context, &event, SACN_RECEIVER_READ_TIMEOUT_MS);
  if (poll_res == kEtcPalErrOk)
  {
    if (event.events & ETCPAL_POLL_ERR)
    {
      etcpal_poll_remove_socket(&recv_thread_context->poll_context, event.socket);
      return event.err;
    }
    else if (event.events & ETCPAL_POLL_IN)
    {
      int recv_res = etcpal_recvfrom(event.socket, recv_thread_context->recv_buf, SACN_MTU, 0, &read_result->from_addr);
      if (recv_res > 0)
      {
        read_result->data_len = (size_t)recv_res;
        read_result->data = recv_thread_context->recv_buf;
        return kEtcPalErrOk;
      }
      else if (recv_res < 0)
      {
        etcpal_poll_remove_socket(&recv_thread_context->poll_context, event.socket);
        return (etcpal_error_t)recv_res;
      }
    }
  }
  return poll_res;
}

void sacn_send_multicast(uint16_t universe_id, sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                         const EtcPalMcastNetintId* netint)
{
  if ((ip_supported == kSacnIpV4Only) || (ip_supported == kSacnIpV4AndIpV6))
    send_multicast(universe_id, kEtcPalIpTypeV4, send_buf, netint);
  if ((ip_supported == kSacnIpV6Only) || (ip_supported == kSacnIpV4AndIpV6))
    send_multicast(universe_id, kEtcPalIpTypeV6, send_buf, netint);
}

void sacn_send_unicast(sacn_ip_support_t ip_supported, const uint8_t* send_buf, const EtcPalIpAddr* dest_addr)
{
  if (((ip_supported == kSacnIpV4Only) || (ip_supported == kSacnIpV4AndIpV6)) && (dest_addr->type == kEtcPalIpTypeV4))
    send_unicast(send_buf, dest_addr);
  if (((ip_supported == kSacnIpV6Only) || (ip_supported == kSacnIpV4AndIpV6)) && (dest_addr->type == kEtcPalIpTypeV6))
    send_unicast(send_buf, dest_addr);
}

SacnSocketsSysNetints* sacn_sockets_get_sys_netints(networking_type_t type)
{
  SacnSocketsSysNetints* sys_netints = NULL;
  switch (type)
  {
    case kReceiver:
      sys_netints = &receiver_sys_netints;
      break;
    case kSourceDetector:
      sys_netints = &source_detector_sys_netints;
      break;
    case kSource:
      sys_netints = &source_sys_netints;
      break;
  }

  return sys_netints;
}

etcpal_error_t sacn_initialize_receiver_netints(SacnInternalNetintArray* receiver_netints,
                                                const SacnNetintConfig* app_netint_config)
{
  return sacn_initialize_internal_netints(receiver_netints, app_netint_config, receiver_sys_netints.sys_netints,
                                          receiver_sys_netints.num_sys_netints);
}

etcpal_error_t sacn_initialize_source_detector_netints(SacnInternalNetintArray* source_detector_netints,
                                                       const SacnNetintConfig* app_netint_config)
{
  return sacn_initialize_internal_netints(source_detector_netints, app_netint_config,
                                          source_detector_sys_netints.sys_netints,
                                          source_detector_sys_netints.num_sys_netints);
}

etcpal_error_t sacn_initialize_source_netints(SacnInternalNetintArray* source_netints,
                                              const SacnNetintConfig* app_netint_config)
{
  return sacn_initialize_internal_netints(source_netints, app_netint_config, source_sys_netints.sys_netints,
                                          source_sys_netints.num_sys_netints);
}

etcpal_error_t sockets_init(const SacnNetintConfig* netint_config, networking_type_t net_type)
{
  SacnSocketsSysNetints* sys_netints = sacn_sockets_get_sys_netints(net_type);

  SACN_ASSERT(sys_netints);
  SACN_ASSERT(sys_netints->num_sys_netints == 0);

  size_t total_netints = (netint_config && netint_config->num_netints > 0) ? netint_config->num_netints
                                                                           : etcpal_netint_get_num_interfaces();
  if (total_netints == 0)
    return kEtcPalErrNoNetints;

#if SACN_DYNAMIC_MEM
  if (net_type == kSource)
  {
    multicast_send_sockets = calloc(total_netints, sizeof(etcpal_socket_t));
    if (!multicast_send_sockets)
      return kEtcPalErrNoMem;
  }
  sys_netints->sys_netints = calloc(total_netints, sizeof(SacnMcastInterface));
  if (!sys_netints->sys_netints)
    return kEtcPalErrNoMem;
#else
  if (total_netints > SACN_MAX_NETINTS)
    total_netints = SACN_MAX_NETINTS;
#endif

  size_t num_valid_sys_netints = apply_netint_config(netint_config, total_netints, sys_netints, net_type);
  if (num_valid_sys_netints == 0)
  {
    SACN_LOG_CRIT("None of the network interfaces were usable for the sACN API.");
    return kEtcPalErrNoNetints;
  }

  if (net_type == kSource)
    return init_unicast_send_sockets();

  return kEtcPalErrOk;
}

etcpal_error_t sockets_reset(const SacnNetintConfig* netint_config, networking_type_t net_type)
{
  etcpal_error_t res = (netint_config && !netints_valid(netint_config->netints, netint_config->num_netints))
                           ? kEtcPalErrInvalid
                           : kEtcPalErrOk;

  if (res == kEtcPalErrOk)
  {
    switch (net_type)
    {
      case kReceiver:
        CLEAR_BUF(&receiver_sys_netints, sys_netints);
        break;
      case kSourceDetector:
        CLEAR_BUF(&source_detector_sys_netints, sys_netints);
        break;
      case kSource:
        clear_source_networking();
        break;
    }

    res = sockets_init(netint_config, net_type);
  }

  return res;
}

void clear_source_networking()
{
  if (ipv4_unicast_send_socket != ETCPAL_SOCKET_INVALID)
    etcpal_close(ipv4_unicast_send_socket);
  if (ipv6_unicast_send_socket != ETCPAL_SOCKET_INVALID)
    etcpal_close(ipv6_unicast_send_socket);

#if SACN_DYNAMIC_MEM
  if (multicast_send_sockets)
#endif
  {
    for (size_t i = 0; i < source_sys_netints.num_sys_netints; ++i)
    {
      if (multicast_send_sockets[i] != ETCPAL_SOCKET_INVALID)
        etcpal_close(multicast_send_sockets[i]);
    }
  }

#if SACN_DYNAMIC_MEM
  if (multicast_send_sockets)
    free(multicast_send_sockets);

  multicast_send_sockets = NULL;
#endif

  CLEAR_BUF(&source_sys_netints, sys_netints);
}

etcpal_error_t validate_netint_config(const SacnNetintConfig* netint_config, const SacnMcastInterface* sys_netints,
                                      size_t num_sys_netints, size_t* num_valid_netints)
{
  *num_valid_netints = 0u;

  if (netint_config && netint_config->netints)
  {
#if !SACN_DYNAMIC_MEM
    if (netint_config->num_netints > SACN_MAX_NETINTS)
      return kEtcPalErrNoMem;
#endif

    for (SacnMcastInterface* netint = netint_config->netints;
         netint < (netint_config->netints + netint_config->num_netints); ++netint)
    {
      if (netints_valid(netint, 1))
      {
        int sys_netint_index = netint_id_index_in_array(&netint->iface, sys_netints, num_sys_netints);

        if (sys_netint_index == -1)
          netint->status = kEtcPalErrNotFound;
        else
          netint->status = sys_netints[sys_netint_index].status;
      }
      else
      {
        netint->status = kEtcPalErrInvalid;
      }

      if (netint->status == kEtcPalErrOk)
        ++(*num_valid_netints);
    }
  }
  else
  {
    for (const SacnMcastInterface* netint = sys_netints; netint < (sys_netints + num_sys_netints); ++netint)
    {
      if (netint->status == kEtcPalErrOk)
        ++(*num_valid_netints);
    }
  }

  return (*num_valid_netints > 0) ? kEtcPalErrOk : kEtcPalErrNoNetints;
}

bool netints_valid(const SacnMcastInterface* netints, size_t num_netints)
{
  bool result = (netints && (num_netints > 0)) || (!netints && (num_netints == 0));

  if (result && netints)
  {
    for (const SacnMcastInterface* netint = netints; netint < (netints + num_netints); ++netint)
    {
      if ((netint->iface.index == 0) || (netint->iface.ip_type == kEtcPalIpTypeInvalid))
        result = false;
    }
  }

  return result;
}

size_t apply_netint_config(const SacnNetintConfig* netint_config, size_t num_netints_to_apply,
                           SacnSocketsSysNetints* sys_netints, networking_type_t net_type)
{
  size_t num_valid_sys_netints = 0;

  if (netint_config && netint_config->netints)
  {
    for (SacnMcastInterface* netint = netint_config->netints; netint < (netint_config->netints + num_netints_to_apply);
         ++netint)
    {
      const EtcPalNetintInfo* matching_netints = NULL;
      size_t num_matching_netints = 0;
      netint->status =
          etcpal_netint_get_interfaces_by_index(netint->iface.index, &matching_netints, &num_matching_netints);

      if (netint->status == kEtcPalErrOk)
      {
        netint->status = test_netints(matching_netints, num_matching_netints,
                                      (netint->iface.ip_type == kEtcPalIpTypeV4) ? kSacnIpV4Only : kSacnIpV6Only,
                                      sys_netints, net_type, &num_valid_sys_netints);
      }
    }
#if !SACN_DYNAMIC_MEM
    for (SacnMcastInterface* netint = (netint_config->netints + num_netints_to_apply);
         netint < (netint_config->netints + netint_config->num_netints); ++netint)
    {
      netint->status = kEtcPalErrNoMem;
    }
#endif
  }
  else
  {
    const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
    test_netints(netint_list, num_netints_to_apply, kSacnIpV4AndIpV6, sys_netints, net_type, &num_valid_sys_netints);
  }

  return num_valid_sys_netints;
}

etcpal_error_t test_netints(const EtcPalNetintInfo* netints, size_t num_netints, sacn_ip_support_t ip_type,
                            SacnSocketsSysNetints* sys_netints, networking_type_t net_type,
                            size_t* num_valid_sys_netints)
{
  etcpal_error_t result = kEtcPalErrOk;

  for (const EtcPalNetintInfo* netint = netints; netint < (netints + num_netints); ++netint)
  {
    if ((ip_type == kSacnIpV4AndIpV6) || ((ip_type == kSacnIpV4Only) && (netint->addr.type == kEtcPalIpTypeV4)) ||
        ((ip_type == kSacnIpV6Only) && (netint->addr.type == kEtcPalIpTypeV6)))
    {
      etcpal_error_t test_result = test_netint(netint, sys_netints, net_type);
      if (test_result == kEtcPalErrOk)
        ++(*num_valid_sys_netints);
      else if (result == kEtcPalErrOk)
        result = test_result;
    }
  }

  return result;
}

etcpal_error_t test_netint(const EtcPalNetintInfo* netint, SacnSocketsSysNetints* sys_netints,
                           networking_type_t net_type)
{
  etcpal_error_t result = kEtcPalErrOk;
  if (net_type == kSource)
  {
    result = test_sacn_source_netint(netint->index, netint->addr.type, &netint->addr);
  }
  else
  {
    result = test_sacn_receiver_netint(netint->index, netint->addr.type, &netint->addr, sys_netints->sys_netints,
                                       &sys_netints->num_sys_netints);
  }

  return result;
}

etcpal_error_t sacn_initialize_internal_netints(SacnInternalNetintArray* internal_netints,
                                                const SacnNetintConfig* app_netint_config,
                                                const SacnMcastInterface* sys_netints, size_t num_sys_netints)
{
  size_t num_valid_netints = 0u;
  etcpal_error_t result = validate_netint_config(app_netint_config, sys_netints, num_sys_netints, &num_valid_netints);

  const SacnMcastInterface* netints_to_use =
      (app_netint_config && app_netint_config->netints) ? app_netint_config->netints : sys_netints;
  size_t num_netints_to_use =
      (app_netint_config && app_netint_config->netints) ? app_netint_config->num_netints : num_sys_netints;

  if (result == kEtcPalErrOk)
  {
    CLEAR_BUF(internal_netints, netints);

#if SACN_DYNAMIC_MEM
    internal_netints->netints = calloc(num_valid_netints, sizeof(EtcPalMcastNetintId));

    if (!internal_netints->netints)
      result = kEtcPalErrNoMem;
    else
      internal_netints->netints_capacity = num_valid_netints;
#endif
  }

  if (result == kEtcPalErrOk)
  {
    for (size_t read_index = 0u, write_index = 0u; read_index < num_netints_to_use; ++read_index)
    {
      if (netints_to_use[read_index].status == kEtcPalErrOk)
      {
        memcpy(&internal_netints->netints[write_index], &netints_to_use[read_index].iface, sizeof(EtcPalMcastNetintId));
        ++write_index;
      }
    }

    internal_netints->num_netints = num_valid_netints;
  }
  else
  {
#if SACN_DYNAMIC_MEM
    internal_netints->netints = NULL;
    internal_netints->netints_capacity = 0;
#endif
    internal_netints->num_netints = 0;
  }

  return result;
}

etcpal_error_t test_sacn_receiver_netint(unsigned int index, etcpal_iptype_t ip_type, const EtcPalIpAddr* addr,
                                         SacnMcastInterface* sys_netints, size_t* num_sys_netints)
{
  // Create a test receive socket on each network interface. If it fails, we remove that interface from the respective
  // set.
  EtcPalMcastNetintId netint_id;
  netint_id.index = index;
  netint_id.ip_type = ip_type;

  // Try creating and subscribing a multicast receive socket.
  // Test receive sockets using an sACN multicast address.
  EtcPalGroupReq greq;
  greq.ifindex = netint_id.index;
  sacn_get_mcast_addr(netint_id.ip_type, 1, &greq.group);

  ReceiveSocket test_socket;
  etcpal_error_t test_res = create_receive_socket(netint_id.ip_type, NULL, false, &test_socket);

  if (test_res == kEtcPalErrOk)
  {
    test_res = subscribe_on_single_interface(test_socket.handle, &greq);

    if (test_res == kEtcPalErrOk)
      test_res = unsubscribe_on_single_interface(test_socket.handle, &greq);

    etcpal_close(test_socket.handle);
  }

  add_sacn_sys_netint(&netint_id, test_res, sys_netints, num_sys_netints);

  if (test_res != kEtcPalErrOk)
  {
    char addr_str[ETCPAL_IP_STRING_BYTES];
    addr_str[0] = '\0';
    if (SACN_CAN_LOG(ETCPAL_LOG_INFO))
      etcpal_ip_to_string(addr, addr_str);

    SACN_LOG_WARNING(
        "Error creating multicast test receive socket on network interface %s: '%s'. This network interface will not "
        "be used for the sACN Receiver.",
        addr_str, etcpal_strerror(test_res));
  }

  return test_res;
}

etcpal_error_t test_sacn_source_netint(unsigned int index, etcpal_iptype_t ip_type, const EtcPalIpAddr* addr)
{
  // Create a test send socket on each network interface. If it fails, we remove that interface from the respective
  // set.
  EtcPalMcastNetintId netint_id;
  netint_id.index = index;
  netint_id.ip_type = ip_type;

  // create_multicast_send_socket() also tests setting the relevant send socket options and the
  // MULTICAST_IF on the relevant interface.
  etcpal_socket_t test_socket;
  etcpal_error_t test_res = create_multicast_send_socket(&netint_id, &test_socket);

  if (test_res == kEtcPalErrOk)
    etcpal_close(test_socket);

  add_sacn_source_sys_netint(&netint_id, test_res);

  if (test_res != kEtcPalErrOk)
  {
    char addr_str[ETCPAL_IP_STRING_BYTES];
    addr_str[0] = '\0';
    if (SACN_CAN_LOG(ETCPAL_LOG_INFO))
      etcpal_ip_to_string(addr, addr_str);

    SACN_LOG_WARNING(
        "Error creating multicast test send socket on network interface %s: '%s'. This network interface will not be "
        "used for the sACN Source.",
        addr_str, etcpal_strerror(test_res));
  }

  return test_res;
}

etcpal_error_t init_unicast_send_sockets()
{
  etcpal_error_t result = create_unicast_send_socket(kEtcPalIpTypeV4, &ipv4_unicast_send_socket);
  if (result == kEtcPalErrOk)
  {
    result = create_unicast_send_socket(kEtcPalIpTypeV6, &ipv6_unicast_send_socket);
    if (result != kEtcPalErrOk)
    {
      etcpal_close(ipv4_unicast_send_socket);
      ipv4_unicast_send_socket = ETCPAL_SOCKET_INVALID;
      ipv6_unicast_send_socket = ETCPAL_SOCKET_INVALID;
    }
  }
  else
  {
    ipv4_unicast_send_socket = ETCPAL_SOCKET_INVALID;
    ipv6_unicast_send_socket = ETCPAL_SOCKET_INVALID;
  }

  return result;
}

bool add_sacn_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status, SacnMcastInterface* sys_netints,
                         size_t* num_sys_netints)
{
  bool added = false;

#if !SACN_DYNAMIC_MEM
  SACN_ASSERT(num_sys_netints);
  SACN_ASSERT((*num_sys_netints) < SACN_MAX_NETINTS);
#endif

  if (netint_id_index_in_array(netint_id, sys_netints, (*num_sys_netints)) == -1)
  {
    sys_netints[*num_sys_netints].iface = *netint_id;
    sys_netints[*num_sys_netints].status = status;

    ++(*num_sys_netints);
    added = true;
  }
  // Else already added - don't add it again

  return added;
}

void add_sacn_source_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status)
{
  if (add_sacn_sys_netint(netint_id, status, source_sys_netints.sys_netints, &source_sys_netints.num_sys_netints))
  {
    if (status == kEtcPalErrOk)
      create_multicast_send_socket(netint_id, &multicast_send_sockets[source_sys_netints.num_sys_netints - 1]);
    else
      multicast_send_sockets[source_sys_netints.num_sys_netints - 1] = ETCPAL_SOCKET_INVALID;
  }
  // Else already added - don't add it again
}

int netint_id_index_in_array(const EtcPalMcastNetintId* id, const SacnMcastInterface* array, size_t array_size)
{
  for (size_t i = 0; i < array_size; ++i)
  {
    if ((array[i].iface.index == id->index) && (array[i].iface.ip_type == id->ip_type))
      return (int)i;
  }
  return -1;
}
