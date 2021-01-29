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

#ifndef SACN_PRIVATE_SOCKETS_H_
#define SACN_PRIVATE_SOCKETS_H_

#include <stdbool.h>
#include <stddef.h>
#include "etcpal/socket.h"
#include "sacn/common.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SacnReadResult
{
  uint8_t* data;
  size_t data_len;
  EtcPalSockAddr from_addr;
} SacnReadResult;

etcpal_error_t sacn_sockets_init(void);
void sacn_sockets_deinit(void);

etcpal_error_t sacn_initialize_receiver_netints(SacnInternalNetintArray* receiver_netints,
                                                SacnMcastInterface* app_netints, size_t num_app_netints);
etcpal_error_t sacn_initialize_source_netints(SacnInternalNetintArray* source_netints, SacnMcastInterface* app_netints,
                                              size_t num_app_netints);

void sacn_get_mcast_addr(etcpal_iptype_t ip_type, uint16_t universe, EtcPalIpAddr* ip);
etcpal_error_t sacn_add_receiver_socket(sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t universe,
                                        const EtcPalMcastNetintId* netints, size_t num_netints,
                                        etcpal_socket_t* socket);
void sacn_remove_receiver_socket(sacn_thread_id_t thread_id, etcpal_socket_t* socket, bool close_now);

// Functions to be called from the receive thread
void sacn_add_pending_sockets(SacnRecvThreadContext* recv_thread_context);
void sacn_cleanup_dead_sockets(SacnRecvThreadContext* recv_thread_context);
etcpal_error_t sacn_read(SacnRecvThreadContext* recv_thread_context, SacnReadResult* read_result);
void sacn_send_multicast(uint16_t universe_id, sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                         const EtcPalMcastNetintId* netint);
void sacn_send_unicast(sacn_ip_support_t ip_supported, const uint8_t* send_buf, const EtcPalIpAddr* dest_addr);

#ifdef __cplusplus
}
#endif

#endif  // SACN_PRIVATE_SOCKETS_H_
