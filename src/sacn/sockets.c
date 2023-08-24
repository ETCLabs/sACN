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

// These are defined before the includes to enable ETCPAL_MAX_CONTROL_SIZE_PKTINFO support on Mac & Linux.
#if defined(__linux__) || defined(__APPLE__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif  // _GNU_SOURCE
#endif  // defined(__linux__) || defined(__APPLE__)

#if defined(__APPLE__)
#ifndef __APPLE_USE_RFC_3542
#define __APPLE_USE_RFC_3542
#endif  // __APPLE_USE_RFC_3542
#endif  // defined(__APPLE__)

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

#include <stdio.h>

/****************************** Private macros *******************************/

#ifdef _MSC_VER
#define SACN_SPRINTF __pragma(warning(suppress : 4996)) sprintf
#else
#define SACN_SPRINTF sprintf
#endif

/****************************** Private types ********************************/

typedef struct MulticastSendSocket
{
  etcpal_socket_t socket;
  etcpal_error_t last_send_error;
} MulticastSendSocket;

typedef struct SysNetintList
{
  SACN_DECLARE_BUF(EtcPalNetintInfo, netints, SACN_MAX_NETINTS);
  size_t num_netints;
} SysNetintList;

/**************************** Private variables ******************************/

#if SACN_DYNAMIC_MEM
static MulticastSendSocket* multicast_send_sockets = NULL;
#else
static MulticastSendSocket multicast_send_sockets[SACN_MAX_NETINTS];
#endif
static SacnSocketsSysNetints receiver_sys_netints;
static SacnSocketsSysNetints source_detector_sys_netints;
static SacnSocketsSysNetints source_sys_netints;
static etcpal_socket_t ipv4_unicast_send_socket = ETCPAL_SOCKET_INVALID;
static etcpal_socket_t ipv6_unicast_send_socket = ETCPAL_SOCKET_INVALID;

/*********************** Private function prototypes *************************/

static etcpal_error_t sockets_init(const SacnNetintConfig* netint_config, networking_type_t net_type);
static etcpal_error_t sockets_reset(const SacnNetintConfig* netint_config, networking_type_t net_type);
static void clear_source_networking();
#if SACN_RECEIVER_ENABLED
static etcpal_error_t update_sampling_period_netints(SacnInternalNetintArray* receiver_netints, bool currently_sampling,
                                                     EtcPalRbTree* sampling_period_netints,
                                                     const SacnNetintConfig* app_netint_config);
#endif  // SACN_RECEIVER_ENABLED
static bool netints_valid(const SacnMcastInterface* netints, size_t num_netints);
static size_t apply_netint_config(const SacnNetintConfig* netint_config, SysNetintList* netint_list,
                                  SacnSocketsSysNetints* sys_netints, networking_type_t net_type);
static etcpal_error_t test_netint(const EtcPalNetintInfo* netint, SacnSocketsSysNetints* sys_netints,
                                  networking_type_t net_type);
static etcpal_error_t test_sacn_receiver_netint(unsigned int index, etcpal_iptype_t ip_type, const EtcPalIpAddr* addr,
                                                SacnSocketsSysNetints* sys_netints);
static etcpal_error_t test_sacn_source_netint(unsigned int index, etcpal_iptype_t ip_type, const EtcPalIpAddr* addr);
static etcpal_error_t init_unicast_send_sockets();
static bool add_sacn_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status,
                                SacnSocketsSysNetints* sys_netints);
static bool add_sacn_source_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status,
                                       etcpal_socket_t socket);
static int netint_id_index_in_array(const EtcPalMcastNetintId* id, const SacnMcastInterface* array, size_t array_size);

static etcpal_error_t create_multicast_send_socket(const EtcPalMcastNetintId* netint_id, etcpal_socket_t* socket);
static etcpal_error_t create_unicast_send_socket(etcpal_iptype_t ip_type, etcpal_socket_t* socket);
#if SACN_FULL_OS_AVAILABLE_HINT
static void configure_sndbuf_size(etcpal_socket_t new_sock, const char* sock_desc);
#endif
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
static etcpal_error_t send_multicast(uint16_t universe_id, const uint8_t* send_buf, const EtcPalMcastNetintId* netint);
static etcpal_error_t send_unicast(const uint8_t* send_buf, const EtcPalIpAddr* dest_addr,
                                   etcpal_error_t* last_send_error);
#if SACN_RECEIVER_ENABLED
static EtcPalSockAddr get_bind_address(etcpal_iptype_t ip_type);
static bool get_netint_id(EtcPalMsgHdr* msg, EtcPalMcastNetintId* netint_id);
#endif  // SACN_RECEIVER_ENABLED

static etcpal_error_t init_sys_netint_list(SysNetintList* netint_list);
static void deinit_sys_netint_list(SysNetintList* netint_list);
static etcpal_error_t populate_sys_netint_list(SysNetintList* netint_list);

static etcpal_error_t get_netint_ip_string(etcpal_iptype_t ip_type, unsigned int index, char* dest);

/*************************** Function definitions ****************************/

etcpal_error_t sacn_sockets_init(const SacnNetintConfig* netint_config)
{
  memset(&receiver_sys_netints, 0, sizeof(receiver_sys_netints));
  memset(&source_detector_sys_netints, 0, sizeof(source_detector_sys_netints));
  memset(&source_sys_netints, 0, sizeof(source_sys_netints));

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
  if (!SACN_ASSERT_VERIFY(recv_thread_context))
    return;

#if SACN_RECEIVER_SOCKET_PER_NIC
  if (!SACN_ASSERT_VERIFY(num_netints <= 1))
    return;
#endif

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
  if (!SACN_ASSERT_VERIFY(context) || !SACN_ASSERT_VERIFY(socket))
    return;

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

etcpal_error_t send_multicast(uint16_t universe_id, const uint8_t* send_buf, const EtcPalMcastNetintId* netint)
{
  if (!SACN_ASSERT_VERIFY(send_buf) || !SACN_ASSERT_VERIFY(netint) ||
      !SACN_ASSERT_VERIFY(netint->ip_type != kEtcPalIpTypeInvalid))
  {
    return kEtcPalErrSys;
  }

  // Determine the multicast destination
  EtcPalSockAddr dest;
  sacn_get_mcast_addr(netint->ip_type, universe_id, &dest.ip);
  dest.port = SACN_PORT;

  // Determine the socket to use
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  etcpal_error_t* last_send_error = NULL;
  int sys_netint_index =
      netint_id_index_in_array(netint, source_sys_netints.sys_netints, source_sys_netints.num_sys_netints);
  if ((sys_netint_index >= 0) && (sys_netint_index < (int)source_sys_netints.num_sys_netints))
  {
    sock = multicast_send_sockets[sys_netint_index].socket;
    last_send_error = &multicast_send_sockets[sys_netint_index].last_send_error;
  }

  if (sock == ETCPAL_SOCKET_INVALID)
    return kEtcPalErrNotInit;

  // Try to send the data (ignore errors)
  const size_t send_buf_length =
      (size_t)ACN_UDP_PREAMBLE_SIZE + (size_t)ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE]));

  etcpal_error_t res = kEtcPalErrOk;
  int sendto_res = etcpal_sendto(sock, send_buf, send_buf_length, 0, &dest);
  if (sendto_res < 0)
    res = (etcpal_error_t)sendto_res;

  if ((res != kEtcPalErrOk) && (res != *last_send_error))
  {
    char netint_addr[ETCPAL_IP_STRING_BYTES] = {'\0'};
    get_netint_ip_string(netint->ip_type, netint->index, netint_addr);

    SACN_LOG_WARNING("Multicast send on network interface %s failed at least once with error '%s'.", netint_addr,
                     etcpal_strerror(res));

    *last_send_error = res;
  }

  return res;
}

etcpal_error_t send_unicast(const uint8_t* send_buf, const EtcPalIpAddr* dest_addr, etcpal_error_t* last_send_error)

{
  if (!SACN_ASSERT_VERIFY(send_buf) || !SACN_ASSERT_VERIFY(dest_addr) || !SACN_ASSERT_VERIFY(last_send_error))
    return kEtcPalErrSys;

  // Determine the socket to use
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  if (dest_addr->type == kEtcPalIpTypeV4)
    sock = ipv4_unicast_send_socket;
  else if (dest_addr->type == kEtcPalIpTypeV6)
    sock = ipv6_unicast_send_socket;

  if (sock == ETCPAL_SOCKET_INVALID)
    return kEtcPalErrNotInit;

  // Convert destination to SockAddr
  EtcPalSockAddr sockaddr_dest;
  sockaddr_dest.ip = *dest_addr;
  sockaddr_dest.port = SACN_PORT;

  // Try to send the data (ignore errors)
  const size_t send_buf_length =
      (size_t)ACN_UDP_PREAMBLE_SIZE + (size_t)ACN_PDU_LENGTH((&send_buf[ACN_UDP_PREAMBLE_SIZE]));

  etcpal_error_t res = kEtcPalErrOk;
  int sendto_res = etcpal_sendto(sock, send_buf, send_buf_length, 0, &sockaddr_dest);
  if (sendto_res < 0)
    res = (etcpal_error_t)sendto_res;

  if ((res != kEtcPalErrOk) && (res != *last_send_error))
  {
    char addr_str[ETCPAL_IP_STRING_BYTES] = {'\0'};
    etcpal_ip_to_string(dest_addr, addr_str);
    SACN_LOG_WARNING("Unicast send to %s failed at least once with error '%s'.", addr_str, etcpal_strerror(res));

    *last_send_error = res;
  }

  return res;
}

#if SACN_RECEIVER_ENABLED

EtcPalSockAddr get_bind_address(etcpal_iptype_t ip_type)
{
  EtcPalSockAddr recv_any;
  etcpal_ip_set_wildcard(ip_type, &recv_any.ip);
  recv_any.port = SACN_PORT;
  return recv_any;
}

bool get_netint_id(EtcPalMsgHdr* msg, EtcPalMcastNetintId* netint_id)
{
  if (!SACN_ASSERT_VERIFY(msg) || !SACN_ASSERT_VERIFY(netint_id))
    return false;

  EtcPalCMsgHdr cmsg = {0};
  EtcPalPktInfo pktinfo = {{0}};
  bool pktinfo_found = false;
  if (etcpal_cmsg_firsthdr(msg, &cmsg))
  {
    do
    {
      pktinfo_found = etcpal_cmsg_to_pktinfo(&cmsg, &pktinfo);
    } while (!pktinfo_found && etcpal_cmsg_nxthdr(msg, &cmsg, &cmsg));
  }

  if (pktinfo_found)
  {
    netint_id->index = pktinfo.ifindex;
    netint_id->ip_type = pktinfo.addr.type;
  }

  return pktinfo_found;
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
  if (!SACN_ASSERT_VERIFY(netint_id) || !SACN_ASSERT_VERIFY(socket))
    return kEtcPalErrSys;

  int sockopt_ip_level = (netint_id->ip_type == kEtcPalIpTypeV6) ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP;
  char* sockopt_ip_level_str = (netint_id->ip_type == kEtcPalIpTypeV6) ? "IPv6" : "IPv4";

  if (!SACN_CAN_LOG(ETCPAL_LOG_ERR))
    ETCPAL_UNUSED_ARG(sockopt_ip_level_str);

  etcpal_socket_t new_sock = ETCPAL_SOCKET_INVALID;
  etcpal_error_t res = etcpal_socket(netint_id->ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET,
                                     ETCPAL_SOCK_DGRAM, &new_sock);

  if (res == kEtcPalErrOk)
  {
    const int ttl = SACN_SOURCE_MULTICAST_TTL;
    res = etcpal_setsockopt(new_sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_TTL, &ttl, sizeof ttl);

    if (res != kEtcPalErrOk)
    {
      SACN_LOG_ERR("Failed to set %s IP_MULTICAST_TTL socket option to %d: '%s'", sockopt_ip_level_str, ttl,
                   etcpal_strerror(res));
    }
  }

  if (res == kEtcPalErrOk)
  {
    res = etcpal_setsockopt(new_sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_IF, &netint_id->index,
                            sizeof netint_id->index);

    if (res != kEtcPalErrOk)
    {
      SACN_LOG_ERR("Failed to set %s IP_MULTICAST_IF socket option to %u: '%s'", sockopt_ip_level_str, netint_id->index,
                   etcpal_strerror(res));
    }
  }

#if SACN_LOOPBACK
  if (res == kEtcPalErrOk)
  {
    int loopback = 1;
    res = etcpal_setsockopt(new_sock, sockopt_ip_level, ETCPAL_IP_MULTICAST_LOOP, &loopback, sizeof loopback);

    if (res != kEtcPalErrOk)
    {
      SACN_LOG_ERR("Failed to enable %s IP_MULTICAST_LOOP socket option: '%s'", sockopt_ip_level_str,
                   etcpal_strerror(res));
    }
  }
#endif

  if (res == kEtcPalErrOk)
  {
#if SACN_FULL_OS_AVAILABLE_HINT
    char netint_addr[ETCPAL_IP_STRING_BYTES] = {'\0'};
    get_netint_ip_string(netint_id->ip_type, netint_id->index, netint_addr);

    char sock_desc[100] = {'\0'};
    const char* ip_type_desc = (netint_id->ip_type == kEtcPalIpTypeV4) ? "IPv4" : "IPv6";
    SACN_SPRINTF(sock_desc, "%s multicast socket for network interface %s", ip_type_desc, netint_addr);
    configure_sndbuf_size(new_sock, sock_desc);
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
  if (!SACN_ASSERT_VERIFY(ip_type != kEtcPalIpTypeInvalid) || !SACN_ASSERT_VERIFY(socket))
    return kEtcPalErrSys;

  etcpal_error_t res =
      etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, socket);

  if (res == kEtcPalErrOk)
  {
#if SACN_FULL_OS_AVAILABLE_HINT
    configure_sndbuf_size(*socket, (ip_type == kEtcPalIpTypeV4) ? "IPv4 unicast socket" : "IPv6 unicast socket");
#endif
  }

  return res;
}

#if SACN_FULL_OS_AVAILABLE_HINT
void configure_sndbuf_size(etcpal_socket_t new_sock, const char* sock_desc)
{
#if !SACN_LOGGING_ENABLED
  ETCPAL_UNUSED_ARG(sock_desc);
#endif

  int set_so_sndbuf_val = SACN_SOURCE_SOCKET_SNDBUF_SIZE;
  etcpal_error_t set_so_sndbuf_res =
      etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_SNDBUF, &set_so_sndbuf_val, sizeof set_so_sndbuf_val);

  if (set_so_sndbuf_res != kEtcPalErrOk)
  {
    SACN_LOG_ERR("Error setting send buffer size to %d on %s: '%s'", set_so_sndbuf_val, sock_desc,
                 etcpal_strerror(set_so_sndbuf_res));
  }

  int get_so_sndbuf_val = 0;
  size_t get_so_sndbuf_size = sizeof(get_so_sndbuf_val);
  etcpal_error_t get_so_sndbuf_res =
      etcpal_getsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_SNDBUF, &get_so_sndbuf_val, &get_so_sndbuf_size);

  if (get_so_sndbuf_res != kEtcPalErrOk)
  {
    SACN_LOG_WARNING("Couldn't verify send buffer size of %s: '%s'", sock_desc, etcpal_strerror(get_so_sndbuf_res));
  }

  if (get_so_sndbuf_val < set_so_sndbuf_val)
  {
    SACN_LOG_WARNING(
        "Couldn't set the desired send buffer size on %s: The desired size was %d, but it ended up being %d.",
        sock_desc, set_so_sndbuf_val, get_so_sndbuf_val);
  }
  else if (get_so_sndbuf_val > set_so_sndbuf_val)
  {
    SACN_LOG_NOTICE("The buffer size for %s was configured to %d, but it ended up being %d.", sock_desc,
                    set_so_sndbuf_val, get_so_sndbuf_val);
  }
}
#endif

etcpal_error_t create_receive_socket(etcpal_iptype_t ip_type, const EtcPalSockAddr* bind_addr, bool set_sockopts,
                                     ReceiveSocket* socket)
{
  if (!SACN_ASSERT_VERIFY(ip_type != kEtcPalIpTypeInvalid) || !SACN_ASSERT_VERIFY(socket))
    return kEtcPalErrSys;

  etcpal_socket_t new_sock;
  etcpal_error_t res =
      etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, &new_sock);
  if (res == kEtcPalErrOk)
  {
    if (set_sockopts)
    {
      int intval = 1;
      res = etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEADDR, &intval, sizeof intval);

      if (res == kEtcPalErrInvalid)
        res = kEtcPalErrOk;  // Ignore cases where this sockopt is not supported
      else if (res != kEtcPalErrOk)
        SACN_LOG_ERR("Failed to enable SO_REUSEADDR socket option: '%s'", etcpal_strerror(res));

      if (res == kEtcPalErrOk)
      {
        intval = 1;
        res = etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_REUSEPORT, &intval, sizeof intval);

        if (res == kEtcPalErrInvalid)
          res = kEtcPalErrOk;  // Ignore cases where this sockopt is not supported
        else if (res != kEtcPalErrOk)
          SACN_LOG_ERR("Failed to enable SO_REUSEPORT socket option: '%s'", etcpal_strerror(res));
      }

      if (res == kEtcPalErrOk)
      {
        intval = SACN_RECEIVER_SOCKET_RCVBUF_SIZE;
        etcpal_error_t set_so_rcvbuf_res =
            etcpal_setsockopt(new_sock, ETCPAL_SOL_SOCKET, ETCPAL_SO_RCVBUF, &intval, sizeof intval);

        if (set_so_rcvbuf_res != kEtcPalErrOk)
          SACN_LOG_ERR("Error setting receive buffer size to %d: '%s'", intval, etcpal_strerror(set_so_rcvbuf_res));
      }

#if !SACN_RECEIVER_SOCKET_PER_NIC
      if (res == kEtcPalErrOk)
      {
        intval = 1;
        if (ip_type == kEtcPalIpTypeV6)
        {
          res = etcpal_setsockopt(new_sock, ETCPAL_IPPROTO_IPV6, ETCPAL_IPV6_PKTINFO, &intval, sizeof intval);

          if (res != kEtcPalErrOk)
            SACN_LOG_ERR("Failed to enable IPV6_PKTINFO socket option: '%s'", etcpal_strerror(res));
        }
        else
        {
          res = etcpal_setsockopt(new_sock, ETCPAL_IPPROTO_IP, ETCPAL_IP_PKTINFO, &intval, sizeof intval);

          if (res != kEtcPalErrOk)
            SACN_LOG_ERR("Failed to enable IP_PKTINFO socket option: '%s'", etcpal_strerror(res));
        }
      }
#endif
    }

    if (res == kEtcPalErrOk)
    {
      if (bind_addr)
        res = etcpal_bind(new_sock, bind_addr);
    }

    if (res == kEtcPalErrOk)
    {
      socket->handle = new_sock;
      socket->ip_type = ip_type;
      socket->bound = (bind_addr != NULL);
      socket->polling = false;
      return kEtcPalErrOk;
    }

    etcpal_close(new_sock);
  }

  return res;
}

void poll_add_socket(SacnRecvThreadContext* recv_thread_context, ReceiveSocket* socket)
{
  if (!SACN_ASSERT_VERIFY(recv_thread_context) || !SACN_ASSERT_VERIFY(socket))
    return;

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
  if (!SACN_ASSERT_VERIFY(ip_type == kEtcPalIpTypeV4 || ip_type == kEtcPalIpTypeV6) ||
      !SACN_ASSERT_VERIFY(universe >= 1 && ((universe <= 63999) || (universe == SACN_DISCOVERY_UNIVERSE))) ||
      !SACN_ASSERT_VERIFY(netints) || !SACN_ASSERT_VERIFY(num_netints > 0) || !SACN_ASSERT_VERIFY(socket))
  {
    return kEtcPalErrSys;
  }

#if SACN_RECEIVER_SOCKET_PER_NIC
  if (!SACN_ASSERT_VERIFY(num_netints == 1) || !SACN_ASSERT_VERIFY(netints[0].ip_type == ip_type))
    return kEtcPalErrSys;
#endif

  SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
  if (!SACN_ASSERT_VERIFY(context))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

  EtcPalSockAddr universe_mcast_addr;
  sacn_get_mcast_addr(ip_type, universe, &universe_mcast_addr.ip);
  universe_mcast_addr.port = SACN_PORT;

  // Find or create a shared socket.
  SocketRef* ref = NULL;
#if SACN_RECEIVER_SOCKET_PER_NIC
  int ref_index = find_socket_ref_with_room(context, ip_type, netints[0].index);
#else
  int ref_index = find_socket_ref_with_room(context, ip_type);
#endif
  if (ref_index >= 0)
  {
    ref = &context->socket_refs[ref_index];
    ++ref->refcount;
  }
  else  // Couldn't find a matching shared socket that has room; must create a new one.
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
#if SACN_RECEIVER_SOCKET_PER_NIC
      new_socket.ifindex = netints[0].index;
#endif
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
#if SACN_RECEIVER_SOCKET_PER_NIC
  if (!SACN_ASSERT_VERIFY(num_netints <= 1))
    return;
#endif

  if (SACN_ASSERT_VERIFY(socket != NULL) && SACN_ASSERT_VERIFY(*socket != ETCPAL_SOCKET_INVALID))
  {
    SacnRecvThreadContext* context = get_recv_thread_context(thread_id);

    if (SACN_ASSERT_VERIFY(context))
    {
      int index = find_socket_ref_by_handle(context, *socket);

      if (SACN_ASSERT_VERIFY(index >= 0))
        unsubscribe_socket_ref(context, index, universe, netints, num_netints, cleanup_behavior);
    }

    *socket = ETCPAL_SOCKET_INVALID;
  }
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
  if (!SACN_ASSERT_VERIFY(recv_thread_context) || !SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) ||
      !SACN_ASSERT_VERIFY(group) || !SACN_ASSERT_VERIFY(netints) || !SACN_ASSERT_VERIFY(num_netints > 0))
  {
    return kEtcPalErrSys;
  }

#if SACN_RECEIVER_SOCKET_PER_NIC
  if (!SACN_ASSERT_VERIFY(num_netints == 1))
    return kEtcPalErrSys;
#endif

  etcpal_error_t res = kEtcPalErrNoNetints;

  EtcPalGroupReq greq;
  greq.group = *group;

  for (const EtcPalMcastNetintId* netint = netints; netint < netints + num_netints; ++netint)
  {
#if SACN_RECEIVER_SOCKET_PER_NIC
    if (SACN_ASSERT_VERIFY(netint->ip_type == group->type))
#else
    if (netint->ip_type == group->type)
#endif
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
  if (!SACN_ASSERT_VERIFY(recv_thread_context) || !SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) ||
      !SACN_ASSERT_VERIFY(group))
  {
    return kEtcPalErrSys;
  }

#if SACN_RECEIVER_SOCKET_PER_NIC
  if (!SACN_ASSERT_VERIFY(num_netints == 1))
    return kEtcPalErrSys;
#endif

  etcpal_error_t res = kEtcPalErrNoNetints;

  EtcPalGroupReq greq;
  greq.group = *group;

  for (const EtcPalMcastNetintId* netint = netints; netint < netints + num_netints; ++netint)
  {
#if SACN_RECEIVER_SOCKET_PER_NIC
    if (SACN_ASSERT_VERIFY(netint->ip_type == group->type))
#else
    if (netint->ip_type == group->type)
#endif
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
  if (!SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) || !SACN_ASSERT_VERIFY(group))
    return kEtcPalErrSys;

  etcpal_error_t res =
      etcpal_setsockopt(sock, group->group.type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                        ETCPAL_MCAST_JOIN_GROUP, group, sizeof(EtcPalGroupReq));
  if (res != kEtcPalErrOk)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char mcast_addr[ETCPAL_IP_STRING_BYTES] = {'\0'};
      char netint_addr[ETCPAL_IP_STRING_BYTES] = {'\0'};
      etcpal_ip_to_string(&group->group, mcast_addr);
      get_netint_ip_string(group->group.type, group->ifindex, netint_addr);

      SACN_LOG_WARNING("Error subscribing to multicast address %s on network interface %s: '%s'", mcast_addr,
                       netint_addr, etcpal_strerror(res));
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
  if (!SACN_ASSERT_VERIFY(sock != ETCPAL_SOCKET_INVALID) || !SACN_ASSERT_VERIFY(group))
    return kEtcPalErrSys;

  etcpal_error_t res =
      etcpal_setsockopt(sock, group->group.type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP,
                        ETCPAL_MCAST_LEAVE_GROUP, group, sizeof(EtcPalGroupReq));
  if (res != kEtcPalErrOk)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char mcast_addr[ETCPAL_IP_STRING_BYTES] = {'\0'};
      char netint_addr[ETCPAL_IP_STRING_BYTES] = {'\0'};
      etcpal_ip_to_string(&group->group, mcast_addr);
      get_netint_ip_string(group->group.type, group->ifindex, netint_addr);

      SACN_LOG_WARNING("Error unsubscribing from multicast address %s on network interface %s: '%s'", mcast_addr,
                       netint_addr, etcpal_strerror(res));
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
#if SACN_RECEIVER_ENABLED
  if (!SACN_ASSERT_VERIFY(recv_thread_context) || !SACN_ASSERT_VERIFY(read_result))
    return kEtcPalErrSys;

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
      uint8_t control_buf[ETCPAL_MAX_CONTROL_SIZE_PKTINFO];  // Ancillary data

      EtcPalMsgHdr msg;
      msg.buf = recv_thread_context->recv_buf;
      msg.buflen = SACN_MTU;
      msg.control = control_buf;
      msg.controllen = ETCPAL_MAX_CONTROL_SIZE_PKTINFO;

      int recv_res = etcpal_recvmsg(event.socket, &msg, 0);
      if (recv_res > 0)
      {
        if (msg.flags & ETCPAL_MSG_TRUNC)
        {
          recv_res = kEtcPalErrProtocol;  // No sACN packets should be bigger than SACN_MTU.
        }
        else
        {
          read_result->from_addr = msg.name;
          read_result->data_len = (size_t)recv_res;
          read_result->data = recv_thread_context->recv_buf;

          // Obtain the network interface the packet came in on using one of two configured methods
#if SACN_RECEIVER_SOCKET_PER_NIC
          if (sacn_lock())
          {
            int index = find_socket_ref_by_handle(recv_thread_context, event.socket);

            if (index >= 0)
            {
              read_result->netint.ip_type = recv_thread_context->socket_refs[index].socket.ip_type;
              read_result->netint.index = recv_thread_context->socket_refs[index].socket.ifindex;
            }
            else
            {
              // Data from a socket we just removed (kEtcPalErrNoSockets will not log an error)
              recv_res = kEtcPalErrNoSockets;
            }

            sacn_unlock();
          }
#else   // SACN_RECEIVER_SOCKET_PER_NIC
          if ((msg.flags & ETCPAL_MSG_CTRUNC) || !get_netint_id(&msg, &read_result->netint))
            recv_res = kEtcPalErrSys;
#endif  // SACN_RECEIVER_SOCKET_PER_NIC
        }
      }

      if (recv_res < 0)
      {
        etcpal_poll_remove_socket(&recv_thread_context->poll_context, event.socket);
        return (etcpal_error_t)recv_res;
      }
    }
  }
  return poll_res;
#else   // SACN_RECEIVER_ENABLED
  ETCPAL_UNUSED_ARG(recv_thread_context);
  ETCPAL_UNUSED_ARG(read_result);
  return kEtcPalErrNotImpl;
#endif  // SACN_RECEIVER_ENABLED
}

etcpal_error_t sacn_send_multicast(uint16_t universe_id, sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                   const EtcPalMcastNetintId* netint)
{
  if (!SACN_ASSERT_VERIFY(send_buf) || !SACN_ASSERT_VERIFY(netint) ||
      !SACN_ASSERT_VERIFY(netint->ip_type != kEtcPalIpTypeInvalid))
  {
    return kEtcPalErrSys;
  }

  if ((ip_supported == kSacnIpV4AndIpV6) || ((ip_supported == kSacnIpV4Only) && (netint->ip_type == kEtcPalIpTypeV4)) ||
      ((ip_supported == kSacnIpV6Only) && (netint->ip_type == kEtcPalIpTypeV6)))
  {
    return send_multicast(universe_id, send_buf, netint);
  }

  // Getting here means we've been asked to send on an interface we're not currently configured to use. Just return Ok
  // for now, no error needed here.
  return kEtcPalErrOk;
}

etcpal_error_t sacn_send_unicast(sacn_ip_support_t ip_supported, const uint8_t* send_buf, const EtcPalIpAddr* dest_addr,
                                 etcpal_error_t* last_send_error)
{
  if (!SACN_ASSERT_VERIFY(send_buf) || !SACN_ASSERT_VERIFY(dest_addr) || !SACN_ASSERT_VERIFY(last_send_error))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;
  bool unicast_sent = false;
  if ((ip_supported == kSacnIpV4AndIpV6) || ((ip_supported == kSacnIpV4Only) && (dest_addr->type == kEtcPalIpTypeV4)) ||
      ((ip_supported == kSacnIpV6Only) && (dest_addr->type == kEtcPalIpTypeV6)))
  {
    res = send_unicast(send_buf, dest_addr, last_send_error);
    unicast_sent = true;
  }

  if (!SACN_ASSERT_VERIFY(unicast_sent))
    return kEtcPalErrSys;

  return res;
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

#if SACN_RECEIVER_ENABLED
etcpal_error_t sacn_initialize_receiver_netints(SacnInternalNetintArray* receiver_netints, bool currently_sampling,
                                                EtcPalRbTree* sampling_period_netints,
                                                const SacnNetintConfig* app_netint_config)
{
  size_t num_valid_netints = 0;
  etcpal_error_t res = sacn_validate_netint_config(app_netint_config, receiver_sys_netints.sys_netints,
                                                   receiver_sys_netints.num_sys_netints, &num_valid_netints);

  if (res == kEtcPalErrOk)
  {
    res = update_sampling_period_netints(receiver_netints, currently_sampling, sampling_period_netints,
                                         app_netint_config);
  }

  if (res == kEtcPalErrOk)
  {
    res = sacn_initialize_internal_netints(receiver_netints, app_netint_config, num_valid_netints,
                                           receiver_sys_netints.sys_netints, receiver_sys_netints.num_sys_netints);
  }

  return res;
}

etcpal_error_t sacn_add_all_netints_to_sampling_period(SacnInternalNetintArray* receiver_netints,
                                                       EtcPalRbTree* sampling_period_netints)
{
  etcpal_error_t res = etcpal_rbtree_clear_with_cb(sampling_period_netints, sampling_period_netint_tree_dealloc);

  for (size_t i = 0; (res == kEtcPalErrOk) && (i < receiver_netints->num_netints); ++i)
    res = add_sacn_sampling_period_netint(sampling_period_netints, &receiver_netints->netints[i], false);

  return res;
}
#endif  // SACN_RECEIVER_ENABLED

etcpal_error_t sacn_initialize_source_detector_netints(SacnInternalNetintArray* source_detector_netints,
                                                       const SacnNetintConfig* app_netint_config)
{
  size_t num_valid_netints = 0;
  etcpal_error_t res = sacn_validate_netint_config(app_netint_config, source_detector_sys_netints.sys_netints,
                                                   source_detector_sys_netints.num_sys_netints, &num_valid_netints);
  if (res == kEtcPalErrOk)
  {
    res = sacn_initialize_internal_netints(source_detector_netints, app_netint_config, num_valid_netints,
                                           source_detector_sys_netints.sys_netints,
                                           source_detector_sys_netints.num_sys_netints);
  }

  return res;
}

etcpal_error_t sacn_initialize_source_netints(SacnInternalNetintArray* source_netints,
                                              const SacnNetintConfig* app_netint_config)
{
  size_t num_valid_netints = 0;
  etcpal_error_t res = sacn_validate_netint_config(app_netint_config, source_sys_netints.sys_netints,
                                                   source_sys_netints.num_sys_netints, &num_valid_netints);
  if (res == kEtcPalErrOk)
  {
    res = sacn_initialize_internal_netints(source_netints, app_netint_config, num_valid_netints,
                                           source_sys_netints.sys_netints, source_sys_netints.num_sys_netints);
  }

  return res;
}

etcpal_error_t sockets_init(const SacnNetintConfig* netint_config, networking_type_t net_type)
{
  SacnSocketsSysNetints* sys_netints = sacn_sockets_get_sys_netints(net_type);
  if (!SACN_ASSERT_VERIFY(sys_netints))
    return kEtcPalErrSys;

  SACN_ASSERT_VERIFY(sys_netints->num_sys_netints == 0);

  // Start by initializing netint_list (the list of interfaces on the system)
  SysNetintList netint_list;
  etcpal_error_t res = init_sys_netint_list(&netint_list);

  if (res == kEtcPalErrOk)
    res = populate_sys_netint_list(&netint_list);

    // Next allocate some resources based on the interface count obtained (might be more than we need)
#if SACN_DYNAMIC_MEM
  if ((res == kEtcPalErrOk) && (net_type == kSource))
  {
    multicast_send_sockets = calloc(netint_list.num_netints, sizeof(MulticastSendSocket));
    if (multicast_send_sockets)
    {
      for (size_t i = 0; i < netint_list.num_netints; ++i)
        multicast_send_sockets[i].socket = ETCPAL_SOCKET_INVALID;
    }
    else
    {
      res = kEtcPalErrNoMem;
    }
  }

  if (res == kEtcPalErrOk)
  {
    sys_netints->sys_netints = calloc(netint_list.num_netints, sizeof(SacnMcastInterface));
    if (sys_netints->sys_netints)
      sys_netints->sys_netints_capacity = netint_list.num_netints;
    else
      res = kEtcPalErrNoMem;
  }
#else
  if (res == kEtcPalErrOk)
  {
    if (net_type == kSource)
    {
      memset(multicast_send_sockets, 0, sizeof(multicast_send_sockets));
      for (size_t i = 0; i < SACN_MAX_NETINTS; ++i)
        multicast_send_sockets[i].socket = ETCPAL_SOCKET_INVALID;
    }

    memset(sys_netints->sys_netints, 0, sizeof(sys_netints->sys_netints));
  }
#endif

  // Now iterate the obtained interface list for testing, populating sys_netints, & writing statuses.
  if (res == kEtcPalErrOk)
  {
    size_t num_valid_sys_netints = apply_netint_config(netint_config, &netint_list, sys_netints, net_type);
    if ((num_valid_sys_netints == 0) && !netint_config->no_netints)
    {
      SACN_LOG_CRIT("None of the network interfaces were usable for the sACN API.");
      res = kEtcPalErrNoNetints;
    }
  }

  // Now do some last-minute initialization, then free memory
  if (res == kEtcPalErrOk)
  {
    if (net_type == kSource)
      res = init_unicast_send_sockets();
  }

  deinit_sys_netint_list(&netint_list);

  return res;
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
      if (multicast_send_sockets[i].socket != ETCPAL_SOCKET_INVALID)
      {
        etcpal_close(multicast_send_sockets[i].socket);
        multicast_send_sockets[i].socket = ETCPAL_SOCKET_INVALID;
      }
    }
  }

#if SACN_DYNAMIC_MEM
  if (multicast_send_sockets)
    free(multicast_send_sockets);

  multicast_send_sockets = NULL;
#endif

  CLEAR_BUF(&source_sys_netints, sys_netints);
}

#if SACN_RECEIVER_ENABLED
etcpal_error_t update_sampling_period_netints(SacnInternalNetintArray* receiver_netints, bool currently_sampling,
                                              EtcPalRbTree* sampling_period_netints,
                                              const SacnNetintConfig* app_netint_config)
{
  if (!SACN_ASSERT_VERIFY(receiver_netints) || !SACN_ASSERT_VERIFY(sampling_period_netints))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

  const SacnMcastInterface* netints =
      (app_netint_config && app_netint_config->netints) ? app_netint_config->netints : receiver_sys_netints.sys_netints;
  size_t num_netints = (app_netint_config && app_netint_config->netints) ? app_netint_config->num_netints
                                                                         : receiver_sys_netints.num_sys_netints;

  if (app_netint_config && app_netint_config->no_netints)
    num_netints = 0;  // This will cause all current SP netints to be removed & none added

  // Add new sampling period netints
  for (size_t i = 0; (res == kEtcPalErrOk) && (i < num_netints); ++i)
  {
    if (netints[i].status != kEtcPalErrOk)
      continue;

    bool existing_netint = false;
    for (size_t j = 0; !existing_netint && (j < receiver_netints->num_netints); ++j)
    {
      existing_netint = (receiver_netints->netints[j].ip_type == netints[i].iface.ip_type) &&
                        (receiver_netints->netints[j].index == netints[i].iface.index);
    }

    if (existing_netint)
      continue;

    res = add_sacn_sampling_period_netint(sampling_period_netints, &netints[i].iface, currently_sampling);
  }

  // If currently sampling, remove sampling period netints not present in new list
  if (currently_sampling)
  {
    for (size_t i = 0; (res == kEtcPalErrOk) && (i < receiver_netints->num_netints); ++i)
    {
      bool found = false;
      for (size_t j = 0; !found && (j < num_netints); ++j)
      {
        found = (receiver_netints->netints[i].ip_type == netints[j].iface.ip_type) &&
                (receiver_netints->netints[i].index == netints[j].iface.index);
      }

      if (!found)
      {
        res = remove_sampling_period_netint(sampling_period_netints, &receiver_netints->netints[i]);
        if (res == kEtcPalErrNotFound)
          res = kEtcPalErrOk;  // Removed interfaces might not be in a sampling period currently.
      }
    }
  }

  return res;
}
#endif  // SACN_RECEIVER_ENABLED

etcpal_error_t sacn_validate_netint_config(const SacnNetintConfig* netint_config, const SacnMcastInterface* sys_netints,
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
      if (!netint_config->no_netints && netints_valid(netint, 1))
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
  else if (!netint_config || !netint_config->no_netints)
  {
    for (const SacnMcastInterface* netint = sys_netints; netint < (sys_netints + num_sys_netints); ++netint)
    {
      if (netint->status == kEtcPalErrOk)
        ++(*num_valid_netints);
    }
  }

  return ((*num_valid_netints > 0) || (netint_config && netint_config->no_netints)) ? kEtcPalErrOk
                                                                                    : kEtcPalErrNoNetints;
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

size_t apply_netint_config(const SacnNetintConfig* netint_config, SysNetintList* netint_list,
                           SacnSocketsSysNetints* sys_netints, networking_type_t net_type)
{
  if (!SACN_ASSERT_VERIFY(netint_list) || !SACN_ASSERT_VERIFY(netint_list->netints) || !SACN_ASSERT_VERIFY(sys_netints))
    return 0;

  bool use_all_netints = (!netint_config || ((netint_config->num_netints == 0) && !netint_config->no_netints));

  if (netint_config)
  {
    for (size_t i = 0; i < netint_config->num_netints; ++i)
      netint_config->netints[i].status = netint_config->no_netints ? kEtcPalErrInvalid : kEtcPalErrNotFound;
  }

  size_t num_valid_sys_netints = 0;
  for (size_t i = 0; i < netint_list->num_netints; ++i)
  {
    const EtcPalNetintInfo* netint = &netint_list->netints[i];

    // Find application-specified interface if needed
    SacnMcastInterface* app_netint = NULL;
    if (!use_all_netints && SACN_ASSERT_VERIFY(netint_config) && !netint_config->no_netints)
    {
      for (size_t j = 0; !app_netint && (j < netint_config->num_netints); ++j)
      {
        if ((netint_config->netints[j].iface.index == netint->index) &&
            (netint_config->netints[j].iface.ip_type == netint->addr.type))
        {
          app_netint = &netint_config->netints[j];
        }
      }
    }

    if (use_all_netints || app_netint)
    {
      // Test, write to sys_netints, & write to app_netint->status
      etcpal_error_t test_result = test_netint(netint, sys_netints, net_type);
      if (test_result == kEtcPalErrOk)
        ++num_valid_sys_netints;

      if (app_netint && SACN_ASSERT_VERIFY(netint_config) &&
          ((app_netint->status == kEtcPalErrNotFound) || (app_netint->status == kEtcPalErrOk)))
      {
        for (size_t j = 0; j < netint_config->num_netints; ++j)  // There could be duplicates - write status to all.
        {
          if ((netint_config->netints[j].iface.index == app_netint->iface.index) &&
              (netint_config->netints[j].iface.ip_type == app_netint->iface.ip_type))
          {
            netint_config->netints[j].status = test_result;
          }
        }
      }
    }
  }

  return num_valid_sys_netints;
}

etcpal_error_t test_netint(const EtcPalNetintInfo* netint, SacnSocketsSysNetints* sys_netints,
                           networking_type_t net_type)
{
  if (!SACN_ASSERT_VERIFY(netint) || !SACN_ASSERT_VERIFY(sys_netints))
    return kEtcPalErrSys;

  etcpal_error_t result = kEtcPalErrOk;
  if (net_type == kSource)
  {
    result = test_sacn_source_netint(netint->index, netint->addr.type, &netint->addr);
  }
  else
  {
    result = test_sacn_receiver_netint(netint->index, netint->addr.type, &netint->addr, sys_netints);
  }

  return result;
}

etcpal_error_t sacn_initialize_internal_netints(SacnInternalNetintArray* internal_netints,
                                                const SacnNetintConfig* app_netint_config, size_t num_valid_app_netints,
                                                const SacnMcastInterface* sys_netints, size_t num_sys_netints)
{
  etcpal_error_t result = kEtcPalErrOk;

  const SacnMcastInterface* netints_to_use =
      (app_netint_config && app_netint_config->netints) ? app_netint_config->netints : sys_netints;
  size_t num_netints_to_use =
      (app_netint_config && app_netint_config->netints) ? app_netint_config->num_netints : num_sys_netints;

  if (app_netint_config && app_netint_config->no_netints)
    num_netints_to_use = 0;

  CLEAR_BUF(internal_netints, netints);

#if SACN_DYNAMIC_MEM
  if (app_netint_config && app_netint_config->no_netints)
  {
    internal_netints->netints_capacity = 0;
  }
  else
  {
    internal_netints->netints = calloc(num_valid_app_netints, sizeof(EtcPalMcastNetintId));

    if (!internal_netints->netints)
      result = kEtcPalErrNoMem;
    else
      internal_netints->netints_capacity = num_valid_app_netints;
  }
#else   // SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(num_valid_app_netints);
#endif  // SACN_DYNAMIC_MEM

  if (result == kEtcPalErrOk)
  {
    size_t num_internal_netints = 0u;
    for (size_t read_index = 0u; read_index < num_netints_to_use; ++read_index)
    {
#if !SACN_DYNAMIC_MEM
      if (num_internal_netints >= SACN_MAX_NETINTS)
      {
        result = kEtcPalErrNoMem;
        break;
      }
#endif  // !SACN_DYNAMIC_MEM

      if (netints_to_use[read_index].status == kEtcPalErrOk)
      {
        bool already_added = false;
        for (size_t i = 0u; i < num_internal_netints; ++i)
        {
          if ((internal_netints->netints[i].index == netints_to_use[read_index].iface.index) &&
              (internal_netints->netints[i].ip_type == netints_to_use[read_index].iface.ip_type))
          {
            already_added = true;
            break;
          }
        }

        if (!already_added)
        {
          memcpy(&internal_netints->netints[num_internal_netints], &netints_to_use[read_index].iface,
                 sizeof(EtcPalMcastNetintId));
          ++num_internal_netints;
        }
      }
    }

    internal_netints->num_netints = num_internal_netints;
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
                                         SacnSocketsSysNetints* sys_netints)
{
  if (!SACN_ASSERT_VERIFY(ip_type != kEtcPalIpTypeInvalid) || !SACN_ASSERT_VERIFY(addr) ||
      !SACN_ASSERT_VERIFY(sys_netints))
  {
    return kEtcPalErrSys;
  }

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

  add_sacn_sys_netint(&netint_id, test_res, sys_netints);

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
  if (!SACN_ASSERT_VERIFY(ip_type != kEtcPalIpTypeInvalid) || !SACN_ASSERT_VERIFY(addr))
    return kEtcPalErrSys;

  // Create a test send socket on each network interface. If it fails, we remove that interface from the respective
  // set.
  EtcPalMcastNetintId netint_id;
  netint_id.index = index;
  netint_id.ip_type = ip_type;

  // create_multicast_send_socket() also tests setting the relevant send socket options and the
  // MULTICAST_IF on the relevant interface.
  etcpal_socket_t new_source_socket = ETCPAL_SOCKET_INVALID;
  etcpal_error_t test_res = create_multicast_send_socket(&netint_id, &new_source_socket);

  if (!add_sacn_source_sys_netint(&netint_id, test_res, new_source_socket))
    etcpal_close(new_source_socket);

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

bool add_sacn_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status,
                         SacnSocketsSysNetints* sys_netints)
{
  if (!SACN_ASSERT_VERIFY(netint_id) || !SACN_ASSERT_VERIFY(sys_netints) ||
      !SACN_ASSERT_VERIFY(sys_netints->sys_netints))
    return false;

  bool added = false;

#if SACN_DYNAMIC_MEM
  size_t max_sys_netints = sys_netints->sys_netints_capacity;
#else
  size_t max_sys_netints = SACN_MAX_NETINTS;
#endif

  if (SACN_ASSERT_VERIFY(sys_netints->num_sys_netints < max_sys_netints))
  {
    if (netint_id_index_in_array(netint_id, sys_netints->sys_netints, sys_netints->num_sys_netints) == -1)
    {
      sys_netints->sys_netints[sys_netints->num_sys_netints].iface = *netint_id;
      sys_netints->sys_netints[sys_netints->num_sys_netints].status = status;

      ++sys_netints->num_sys_netints;
      added = true;
    }
    // Else already added - don't add it again
  }
  return added;
}

bool add_sacn_source_sys_netint(const EtcPalMcastNetintId* netint_id, etcpal_error_t status, etcpal_socket_t socket)
{
  if (!SACN_ASSERT_VERIFY(netint_id))
    return false;

  if (add_sacn_sys_netint(netint_id, status, &source_sys_netints))
  {
    multicast_send_sockets[source_sys_netints.num_sys_netints - 1].last_send_error = kEtcPalErrOk;
    multicast_send_sockets[source_sys_netints.num_sys_netints - 1].socket = socket;
    return true;
  }
  // Else already added - don't add it again
  return false;
}

int netint_id_index_in_array(const EtcPalMcastNetintId* id, const SacnMcastInterface* array, size_t array_size)
{
  if (!SACN_ASSERT_VERIFY(id) || !SACN_ASSERT_VERIFY(array))
    return -1;

  for (size_t i = 0; i < array_size; ++i)
  {
    if ((array[i].iface.index == id->index) && (array[i].iface.ip_type == id->ip_type))
      return (int)i;
  }
  return -1;
}

etcpal_error_t init_sys_netint_list(SysNetintList* netint_list)
{
  if (!SACN_ASSERT_VERIFY(netint_list))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  netint_list->netints = calloc(INITIAL_CAPACITY, sizeof(EtcPalNetintInfo));
  if (netint_list->netints)
    netint_list->netints_capacity = INITIAL_CAPACITY;
  else
    return kEtcPalErrNoMem;
#else
  memset(netint_list->netints, 0, sizeof(netint_list->netints));
#endif
  return kEtcPalErrOk;
}

void deinit_sys_netint_list(SysNetintList* netint_list)
{
  if (!SACN_ASSERT_VERIFY(netint_list))
    return;

#if SACN_DYNAMIC_MEM
  if (netint_list->netints)
    free(netint_list->netints);
#endif
}

etcpal_error_t populate_sys_netint_list(SysNetintList* netint_list)
{
  if (!SACN_ASSERT_VERIFY(netint_list))
    return kEtcPalErrSys;

#if SACN_DYNAMIC_MEM
  netint_list->num_netints = 4;  // Start with estimate which eventually has the actual number written to it
#else
  netint_list->num_netints = SACN_MAX_NETINTS;
#endif

  etcpal_error_t res = kEtcPalErrOk;
  do
  {
    res = etcpal_netint_get_interfaces(netint_list->netints, &netint_list->num_netints);
    if (res == kEtcPalErrBufSize)
    {
      CHECK_CAPACITY(netint_list, netint_list->num_netints, netints, EtcPalNetintInfo, SACN_MAX_NETINTS,
                     kEtcPalErrNoMem);
    }
    else if (res == kEtcPalErrNotFound)
    {
      res = kEtcPalErrNoNetints;
    }
  } while (res == kEtcPalErrBufSize);

  return res;
}

etcpal_error_t get_netint_ip_string(etcpal_iptype_t ip_type, unsigned int index, char* dest)
{
  if (!SACN_ASSERT_VERIFY(dest))
    return kEtcPalErrSys;

  SysNetintList netint_list;
  etcpal_error_t res = init_sys_netint_list(&netint_list);

  if (res == kEtcPalErrOk)
    res = populate_sys_netint_list(&netint_list);

  if (res == kEtcPalErrOk)
  {
    res = kEtcPalErrNotFound;
    for (size_t i = 0; i < netint_list.num_netints; ++i)
    {
      if ((netint_list.netints[i].addr.type == ip_type) && (netint_list.netints[i].index == index))
      {
        res = etcpal_ip_to_string(&netint_list.netints[i].addr, dest);
        break;
      }
    }
  }

  deinit_sys_netint_list(&netint_list);

  return res;
}
