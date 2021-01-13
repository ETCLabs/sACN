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

#include "sacn/private/sockets.h"

#include "etcpal/common.h"
#include "etcpal/netint.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/******************************* Private types *******************************/

typedef struct McastSendSocket
{
  etcpal_socket_t socket;
  size_t ref_count;
} McastSendSocket;

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static SacnMcastInterface* sys_netints;
static McastSendSocket* send_sockets;
#else
static SacnMcastInterface sys_netints[SACN_MAX_NETINTS];
static McastSendSocket send_sockets[SACN_MAX_NETINTS];
#endif
static size_t num_sys_netints;

/*********************** Private function prototypes *************************/

static etcpal_error_t test_sacn_netint(const EtcPalMcastNetintId* netint_id, const char* addr_str);
static void add_sacn_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status);
static int netint_id_index_in_array(const EtcPalMcastNetintId* id, const SacnMcastInterface* array, size_t array_size);

static etcpal_error_t create_send_socket(const EtcPalMcastNetintId* netint_id, etcpal_socket_t* socket);
static etcpal_error_t create_receiver_socket(etcpal_iptype_t ip_type, const EtcPalSockAddr* bind_addr,
                                             bool set_sockopts, etcpal_socket_t* socket);
static etcpal_error_t subscribe_receiver_socket(etcpal_socket_t sock, const EtcPalIpAddr* group,
                                                const EtcPalMcastNetintId* netints, size_t num_netints);
static etcpal_error_t subscribe_on_single_interface(etcpal_socket_t sock, const EtcPalGroupReq* group);
static void cleanup_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket, bool close_now);

/*************************** Function definitions ****************************/

etcpal_error_t sacn_sockets_init(void)
{
  SACN_ASSERT(num_sys_netints == 0);

  size_t total_sys_netints = etcpal_netint_get_num_interfaces();
  if (total_sys_netints == 0)
    return kEtcPalErrNoNetints;

#if SACN_DYNAMIC_MEM
  send_sockets = calloc(total_sys_netints, sizeof(McastSendSocket));
  if (!send_sockets)
    return kEtcPalErrNoMem;
  sys_netints = calloc(total_sys_netints, sizeof(SacnMcastInterface));
  if (!sys_netints)
  {
    free(send_sockets);
    return kEtcPalErrNoMem;
  }
#else
  if (total_sys_netints > SACN_MAX_NETINTS)
    total_sys_netints = SACN_MAX_NETINTS;
#endif

  size_t num_valid_sys_netints = 0;

  const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
  for (const EtcPalNetintInfo* netint = netint_list; netint < netint_list + total_sys_netints; ++netint)
  {
    // Get the interface IP address for logging
    char addr_str[ETCPAL_IP_STRING_BYTES];
    addr_str[0] = '\0';
    if (SACN_CAN_LOG(ETCPAL_LOG_INFO))
    {
      etcpal_ip_to_string(&netint->addr, addr_str);
    }

    // Create a test send and receive socket on each network interface. If either one fails, we
    // remove that interface from the final set.
    EtcPalMcastNetintId netint_id;
    netint_id.index = netint->index;
    netint_id.ip_type = netint->addr.type;

    if (test_sacn_netint(&netint_id, addr_str) == kEtcPalErrOk)
      ++num_valid_sys_netints;
  }

  if (num_valid_sys_netints == 0)
  {
    SACN_LOG_CRIT("No usable multicast network interfaces found.");
    return kEtcPalErrNoNetints;
  }

  return kEtcPalErrOk;
}

void sacn_sockets_deinit(void)
{
  for (size_t i = 0; i < num_sys_netints; ++i)
  {
    if (send_sockets[i].ref_count)
      etcpal_close(send_sockets[i].socket);
  }
#if SACN_DYNAMIC_MEM
  free(send_sockets);
  free(sys_netints);
#endif
  num_sys_netints = 0;
}

static void cleanup_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket, bool close_now)
{
  if (remove_socket_ref(recv_thread_context, socket))
  {
    if (close_now)
    {
      etcpal_close(socket);
    }
    else
    {
      // We don't close the socket here, due to potential thread safety issues.
      // It gets added to a queue and closed from the socket read thread.
      add_dead_socket(recv_thread_context, socket);
    }
  }
}

/*
 * Internal function to create a new send socket associated with an interface.
 * There is a one-to-one relationship between interfaces and send sockets.
 *
 * [in] netint_id Network interface identifier.
 * [out] new_sock Filled in with new socket descriptor.
 * Returns kEtcPalErrOk (success) or a relevant error code on failure.
 */
etcpal_error_t create_send_socket(const EtcPalMcastNetintId* netint_id, etcpal_socket_t* socket)
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

etcpal_error_t create_receiver_socket(etcpal_iptype_t ip_type, const EtcPalSockAddr* bind_addr, bool set_sockopts,
                                      etcpal_socket_t* socket)
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

  *socket = new_sock;
  return res;
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
  SACN_ASSERT(universe >= 1 && universe <= 63999);

  SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
  SACN_ASSERT(context);

  etcpal_error_t res = kEtcPalErrOk;
  etcpal_socket_t new_socket = ETCPAL_SOCKET_INVALID;

  EtcPalSockAddr universe_mcast_addr;
  sacn_get_mcast_addr(ip_type, universe, &universe_mcast_addr.ip);
  universe_mcast_addr.port = SACN_PORT;

  // Find a shared socket that has room for another subscription.
  for (SocketRef* entry = context->socket_refs; entry < context->socket_refs + context->num_socket_refs; ++entry)
  {
    if ((entry->ip_type == ip_type) && (entry->refcount < SACN_RECEIVER_MAX_SUBS_PER_SOCKET))
    {
      new_socket = entry->sock;
      ++entry->refcount;
      break;
    }
  }

  // No shared sockets have room; must create a new one.
  if (new_socket == ETCPAL_SOCKET_INVALID)
  {
    // Bind to the wildcard address and sACN port
    EtcPalSockAddr recv_any;
    etcpal_ip_set_wildcard(ip_type, &recv_any.ip);
    recv_any.port = SACN_PORT;

    EtcPalSockAddr* bind_addr = &recv_any;
#if SACN_RECEIVER_LIMIT_BIND
    // Limit IPv4 to one bind and IPv6 to one bind for this thread.
    if (((ip_type != kEtcPalIpTypeV4) || context->ipv4_bound) && ((ip_type != kEtcPalIpTypeV6) || context->ipv6_bound))
      bind_addr = NULL;
    else if (ip_type == kEtcPalIpTypeV4)
      context->ipv4_bound = true;
    else if (ip_type == kEtcPalIpTypeV6)
      context->ipv6_bound = true;
#endif
    res = create_receiver_socket(ip_type, bind_addr, true, &new_socket);
    if (res == kEtcPalErrOk)
    {
      // Try to add the new socket ref to the array
      if (!add_socket_ref(context, new_socket, ip_type, bind_addr != NULL))
      {
        res = kEtcPalErrNoMem;
        etcpal_close(new_socket);
        new_socket = ETCPAL_SOCKET_INVALID;
        SACN_LOG_WARNING("Couldn't allocate memory for new sACN receiver socket!");
      }
    }
  }

  if (res != kEtcPalErrOk)
  {
    SACN_LOG_WARNING("Couldn't create new sACN receiver socket: '%s'", etcpal_strerror(res));
    return res;
  }

  res = subscribe_receiver_socket(new_socket, &universe_mcast_addr.ip, netints, num_netints);
  if (res == kEtcPalErrOk)
    *socket = new_socket;
  else if (new_socket != ETCPAL_SOCKET_INVALID)
    cleanup_socket(context, new_socket, true);

  return res;
}

void sacn_remove_receiver_socket(sacn_thread_id_t thread_id, etcpal_socket_t *socket, bool close_now)
{
  SACN_ASSERT(socket != NULL);
  SACN_ASSERT(*socket != ETCPAL_SOCKET_INVALID);

  SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
  SACN_ASSERT(context);

  cleanup_socket(context, *socket, close_now);

  *socket = ETCPAL_SOCKET_INVALID;
}

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
 * Subscribes a socket to a multicast address on all specified network interfaces.
 *
 * [in] sock Socket for which to do the subscribes.
 * [in] group Multicast group address to subscribe the socket to.
 * [in] netints Array of network interfaces to subscribe on, or NULL to use all available interfaces.
 * [in] num_netints Number of entries in netints array, or 0 to use all available interfaces.
 * Returns kEtcPalErrOk if the subscribe was done successfully, error code otherwise.
 */
etcpal_error_t subscribe_receiver_socket(etcpal_socket_t sock, const EtcPalIpAddr* group,
                                         const EtcPalMcastNetintId* netints, size_t num_netints)
{
  SACN_ASSERT(sock != ETCPAL_SOCKET_INVALID);
  SACN_ASSERT(group);

  etcpal_error_t res = kEtcPalErrOk;

  EtcPalGroupReq greq;
  greq.group = *group;

  if (!netints)
  {
    res = kEtcPalErrNoNetints;
    for (SacnMcastInterface* netint = sys_netints; netint < sys_netints + num_sys_netints; ++netint)
    {
      if ((netint->status == kEtcPalErrOk) && (netint->iface.ip_type == group->type))
      {
        greq.ifindex = netint->iface.index;
        res = subscribe_on_single_interface(sock, &greq);
        if (res != kEtcPalErrOk)
          break;
      }
    }
  }
  else
  {
    SACN_ASSERT(num_netints > 0);

    for (const EtcPalMcastNetintId* netint = netints; netint < netints + num_netints; ++netint)
    {
      if (netint->ip_type == group->type)
      {
        greq.ifindex = netint->index;
        // If the user specified a list of network interfaces, failing to subscribe on any of them
        // is a failure.
        res = subscribe_on_single_interface(sock, &greq);
        if (res != kEtcPalErrOk)
          break;
      }
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
#if SACN_RECEIVER_LIMIT_BIND
      if (recv_thread_context->socket_refs[i].bound)
#endif
      {
        etcpal_error_t add_res = etcpal_poll_add_socket(&recv_thread_context->poll_context,
                                                        recv_thread_context->socket_refs[i].sock, ETCPAL_POLL_IN, NULL);
        if (add_res != kEtcPalErrOk)
        {
          SACN_LOG_ERR(
              "Error adding new socket to sACN poll context: '%s'. sACN Receiver will likely not work correctly.",
              etcpal_strerror(add_res));
        }
      }
    }
  }
  recv_thread_context->new_socket_refs = 0;
}

void sacn_cleanup_dead_sockets(SacnRecvThreadContext* recv_thread_context)
{
  for (etcpal_socket_t* socket = recv_thread_context->dead_sockets;
       socket < recv_thread_context->dead_sockets + recv_thread_context->num_dead_sockets; ++socket)
  {
    etcpal_poll_remove_socket(&recv_thread_context->poll_context, *socket);
    etcpal_close(*socket);
  }
  recv_thread_context->num_dead_sockets = 0;
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

etcpal_error_t sacn_validate_netint_config(SacnMcastInterface* netints, size_t num_netints, size_t* num_valid_netints)
{
  if (num_valid_netints)
    *num_valid_netints = 0u;

  if (netints && (num_netints > 0u))
  {
#if !SACN_DYNAMIC_MEM
    if (num_netints > SACN_MAX_NETINTS)
      return kEtcPalErrNoMem;
#endif

    bool all_interfaces_invalid = true;

    for (SacnMcastInterface* netint = netints; netint < (netints + num_netints); ++netint)
    {
      if ((netint->iface.index == 0) || (netint->iface.ip_type == kEtcPalIpTypeInvalid))
      {
        netint->status = kEtcPalErrInvalid;
      }
      else
      {
        int sys_netint_index = netint_id_index_in_array(&netint->iface, sys_netints, num_sys_netints);

        if (sys_netint_index == -1)
          netint->status = kEtcPalErrNotFound;
        else
          netint->status = sys_netints[sys_netint_index].status;

        if (netint->status == kEtcPalErrOk)
        {
          all_interfaces_invalid = false;

          if (num_valid_netints)
            ++(*num_valid_netints);
        }
      }
    }

    return all_interfaces_invalid ? kEtcPalErrNoNetints : kEtcPalErrOk;
  }
  else if (netints || num_netints > 0)
  {
    // Mismatched
    return kEtcPalErrInvalid;
  }
  else
  {
    return kEtcPalErrOk;
  }
}

etcpal_error_t sacn_initialize_internal_netints(EtcPalMcastNetintId** internal_netints, size_t* num_internal_netints,
                                                SacnMcastInterface* app_netints, size_t num_app_netints)
{
  size_t num_valid_netints = 0u;
  etcpal_error_t result = sacn_validate_netint_config(app_netints, num_app_netints, &num_valid_netints);
  if (result != kEtcPalErrOk)
    return result;

  if (app_netints)
  {
#if SACN_DYNAMIC_MEM
    EtcPalMcastNetintId* calloc_result = calloc(num_valid_netints, sizeof(EtcPalMcastNetintId));

    if (calloc_result)
      *internal_netints = calloc_result;
    else
      result = kEtcPalErrNoMem;
#else
    if (num_app_netints > SACN_MAX_NETINTS)
      result = kEtcPalErrNoMem;
#endif

    if (result == kEtcPalErrOk)
    {
      for (size_t read_index = 0u, write_index = 0u; read_index < num_app_netints; ++read_index)
      {
        if (app_netints[read_index].status == kEtcPalErrOk)
        {
          memcpy(*internal_netints + write_index, &app_netints[read_index].iface, sizeof(EtcPalMcastNetintId));
          ++write_index;
        }
      }
    }
  }
#if SACN_DYNAMIC_MEM
  else
  {
    *internal_netints = NULL;
  }
#endif

  if (result == kEtcPalErrOk)
  {
    *num_internal_netints = num_valid_netints;
  }

  return result;
}

etcpal_error_t test_sacn_netint(const EtcPalMcastNetintId* netint_id, const char* addr_str)
{
#if !SACN_LOGGING_ENABLED
  ETCPAL_UNUSED_ARG(addr_str);
#endif

  // create_send_socket() also tests setting the relevant send socket options and the
  // MULTICAST_IF on the relevant interface.
  etcpal_socket_t test_socket;
  etcpal_error_t test_res = create_send_socket(netint_id, &test_socket);
  if (test_res == kEtcPalErrOk)
  {
    etcpal_close(test_socket);

    // Try creating and subscribing a multicast receive socket.
    // Test receive sockets using an sACN multicast address.
    EtcPalGroupReq greq;
    greq.ifindex = netint_id->index;
    sacn_get_mcast_addr(netint_id->ip_type, 1, &greq.group);

    test_res = create_receiver_socket(netint_id->ip_type, NULL, false, &test_socket);

    if (test_res == kEtcPalErrOk)
    {
      test_res = subscribe_on_single_interface(test_socket, &greq);
      etcpal_close(test_socket);
    }
  }

  add_sacn_netint(netint_id, test_res);

  if (test_res != kEtcPalErrOk)
  {
    SACN_LOG_WARNING(
        "Error creating multicast test socket on network interface %s: '%s'. This network interface will not be used "
        "for sACN.",
        addr_str, etcpal_strerror(test_res));
  }

  return test_res;
}

void add_sacn_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status)
{
#if !SACN_DYNAMIC_MEM
  SACN_ASSERT(num_sys_netints < SACN_MAX_NETINTS);
#endif

  if (netint_id_index_in_array(netint_id, sys_netints, num_sys_netints) == -1)
  {
    sys_netints[num_sys_netints].iface = *netint_id;
    sys_netints[num_sys_netints].status = status;
    send_sockets[num_sys_netints].ref_count = 0;
    send_sockets[num_sys_netints].socket = ETCPAL_SOCKET_INVALID;
    ++num_sys_netints;
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
