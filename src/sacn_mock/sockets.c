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

#include "sacn_mock/private/sockets.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_init, const SacnNetintConfig*);
DEFINE_FAKE_VOID_FUNC(sacn_sockets_deinit);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_reset_source, const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_reset_receiver, const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_reset_source_detector, const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_initialize_receiver_netints, SacnInternalNetintArray*, bool, EtcPalRbTree*,
                       const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_add_all_netints_to_sampling_period, SacnInternalNetintArray*,
                       EtcPalRbTree*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_initialize_source_detector_netints, SacnInternalNetintArray*,
                       const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_initialize_source_netints, SacnInternalNetintArray*,
                       const SacnNetintConfig*);
DEFINE_FAKE_VOID_FUNC(sacn_get_mcast_addr, etcpal_iptype_t, uint16_t, EtcPalIpAddr*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_add_receiver_socket, sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                       const EtcPalMcastNetintId*, size_t, etcpal_socket_t*);
DEFINE_FAKE_VOID_FUNC(sacn_remove_receiver_socket, sacn_thread_id_t, etcpal_socket_t*, uint16_t,
                      const EtcPalMcastNetintId*, size_t, socket_cleanup_behavior_t);
DEFINE_FAKE_VOID_FUNC(sacn_add_pending_sockets, SacnRecvThreadContext*);
DEFINE_FAKE_VOID_FUNC(sacn_cleanup_dead_sockets, SacnRecvThreadContext*);
DEFINE_FAKE_VOID_FUNC(sacn_subscribe_sockets, SacnRecvThreadContext*);
DEFINE_FAKE_VOID_FUNC(sacn_unsubscribe_sockets, SacnRecvThreadContext*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_read, SacnRecvThreadContext*, SacnReadResult*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_send_multicast, uint16_t, sacn_ip_support_t, const uint8_t*,
                       const EtcPalMcastNetintId*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_send_unicast, sacn_ip_support_t, const uint8_t*, const EtcPalIpAddr*,
                       etcpal_error_t*);

void sacn_sockets_reset_all_fakes(void)
{
  RESET_FAKE(sacn_sockets_init);
  RESET_FAKE(sacn_sockets_deinit);
  RESET_FAKE(sacn_sockets_reset_source);
  RESET_FAKE(sacn_sockets_reset_receiver);
  RESET_FAKE(sacn_sockets_reset_source_detector);
  RESET_FAKE(sacn_initialize_receiver_netints);
  RESET_FAKE(sacn_add_all_netints_to_sampling_period);
  RESET_FAKE(sacn_initialize_source_detector_netints);
  RESET_FAKE(sacn_initialize_source_netints);
  RESET_FAKE(sacn_get_mcast_addr);
  RESET_FAKE(sacn_add_receiver_socket);
  RESET_FAKE(sacn_remove_receiver_socket);
  RESET_FAKE(sacn_add_pending_sockets);
  RESET_FAKE(sacn_cleanup_dead_sockets);
  RESET_FAKE(sacn_subscribe_sockets);
  RESET_FAKE(sacn_unsubscribe_sockets);
  RESET_FAKE(sacn_read);
  RESET_FAKE(sacn_send_multicast);
  RESET_FAKE(sacn_send_unicast);
}
