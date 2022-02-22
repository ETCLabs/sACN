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

#ifndef SACN_MOCK_PRIVATE_SOCKETS_H_
#define SACN_MOCK_PRIVATE_SOCKETS_H_

#include "sacn/private/sockets.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_init, const SacnNetintConfig*);
DECLARE_FAKE_VOID_FUNC(sacn_sockets_deinit);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_reset_source, const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_reset_receiver, const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_reset_source_detector, const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_initialize_receiver_netints, SacnInternalNetintArray*,
                        const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_initialize_source_detector_netints, SacnInternalNetintArray*,
                        const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_initialize_source_netints, SacnInternalNetintArray*,
                        const SacnNetintConfig*);
DECLARE_FAKE_VOID_FUNC(sacn_get_mcast_addr, etcpal_iptype_t, uint16_t, EtcPalIpAddr*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_add_receiver_socket, sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                        const EtcPalMcastNetintId*, size_t, etcpal_socket_t*);
DECLARE_FAKE_VOID_FUNC(sacn_remove_receiver_socket, sacn_thread_id_t, etcpal_socket_t*, uint16_t,
                       const EtcPalMcastNetintId*, size_t, socket_cleanup_behavior_t);
DECLARE_FAKE_VOID_FUNC(sacn_add_pending_sockets, SacnRecvThreadContext*);
DECLARE_FAKE_VOID_FUNC(sacn_cleanup_dead_sockets, SacnRecvThreadContext*);
DECLARE_FAKE_VOID_FUNC(sacn_subscribe_sockets, SacnRecvThreadContext*);
DECLARE_FAKE_VOID_FUNC(sacn_unsubscribe_sockets, SacnRecvThreadContext*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_read, SacnRecvThreadContext*, SacnReadResult*);
DECLARE_FAKE_VOID_FUNC(sacn_send_multicast, uint16_t, sacn_ip_support_t, const uint8_t*, const EtcPalMcastNetintId*);
DECLARE_FAKE_VOID_FUNC(sacn_send_unicast, sacn_ip_support_t, const uint8_t*, const EtcPalIpAddr*);

void sacn_sockets_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_SOCKETS_H_ */
