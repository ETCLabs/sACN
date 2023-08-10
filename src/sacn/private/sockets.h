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
  EtcPalMcastNetintId netint;
} SacnReadResult;

typedef struct SacnSocketsSysNetints
{
  SACN_DECLARE_BUF(SacnMcastInterface, sys_netints, SACN_MAX_NETINTS);
  size_t num_sys_netints;
} SacnSocketsSysNetints;

typedef enum
{
  kReceiver,
  kSourceDetector,
  kSource
} networking_type_t;

etcpal_error_t sacn_sockets_init(const SacnNetintConfig* netint_config);
void sacn_sockets_deinit(void);

etcpal_error_t sacn_sockets_reset_source(const SacnNetintConfig* netint_config);
etcpal_error_t sacn_sockets_reset_receiver(const SacnNetintConfig* netint_config);
etcpal_error_t sacn_sockets_reset_source_detector(const SacnNetintConfig* netint_config);

#if SACN_RECEIVER_ENABLED
etcpal_error_t sacn_initialize_receiver_netints(SacnInternalNetintArray* receiver_netints, bool currently_sampling,
                                                EtcPalRbTree* sampling_period_netints,
                                                const SacnNetintConfig* app_netint_config);
etcpal_error_t sacn_add_all_netints_to_sampling_period(SacnInternalNetintArray* receiver_netints,
                                                       EtcPalRbTree* sampling_period_netints);
#endif  // SACN_RECEIVER_ENABLED
etcpal_error_t sacn_initialize_source_detector_netints(SacnInternalNetintArray* source_detector_netints,
                                                       const SacnNetintConfig* app_netint_config);
etcpal_error_t sacn_initialize_source_netints(SacnInternalNetintArray* source_netints,
                                              const SacnNetintConfig* app_netint_config);

etcpal_error_t sacn_validate_netint_config(const SacnNetintConfig* netint_config, const SacnMcastInterface* sys_netints,
                                           size_t num_sys_netints, size_t* num_valid_netints);
etcpal_error_t sacn_initialize_internal_netints(SacnInternalNetintArray* internal_netints,
                                                const SacnNetintConfig* app_netint_config, size_t num_valid_app_netints,
                                                const SacnMcastInterface* sys_netints, size_t num_sys_netints);

etcpal_error_t sacn_initialize_internal_sockets(SacnInternalSocketState* sockets);

void sacn_get_mcast_addr(etcpal_iptype_t ip_type, uint16_t universe, EtcPalIpAddr* ip);
etcpal_error_t sacn_add_receiver_socket(sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t universe,
                                        const EtcPalMcastNetintId* netints, size_t num_netints,
                                        etcpal_socket_t* socket);
void sacn_remove_receiver_socket(sacn_thread_id_t thread_id, etcpal_socket_t* socket, uint16_t universe,
                                 const EtcPalMcastNetintId* netints, size_t num_netints,
                                 socket_cleanup_behavior_t cleanup_behavior);

// Functions to be called from the receive thread
void sacn_add_pending_sockets(SacnRecvThreadContext* recv_thread_context);
void sacn_cleanup_dead_sockets(SacnRecvThreadContext* recv_thread_context);
void sacn_subscribe_sockets(SacnRecvThreadContext* recv_thread_context);
void sacn_unsubscribe_sockets(SacnRecvThreadContext* recv_thread_context);
etcpal_error_t sacn_read(SacnRecvThreadContext* recv_thread_context, SacnReadResult* read_result);

// Source sending functions
etcpal_error_t sacn_send_multicast(uint16_t universe_id, sacn_ip_support_t ip_supported, const uint8_t* send_buf,
                                   const EtcPalMcastNetintId* netint);
etcpal_error_t sacn_send_unicast(sacn_ip_support_t ip_supported, const uint8_t* send_buf, const EtcPalIpAddr* dest_addr,
                                 etcpal_error_t* last_send_error);

// Sys netints getter, exposed here for unit testing
SacnSocketsSysNetints* sacn_sockets_get_sys_netints(networking_type_t type);

#ifdef __cplusplus
}
#endif

#endif  // SACN_PRIVATE_SOCKETS_H_
