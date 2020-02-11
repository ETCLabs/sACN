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

#ifndef SACN_MOCK_PRIVATE_SOCKETS_H_
#define SACN_MOCK_PRIVATE_SOCKETS_H_

#include "sacn/private/sockets.h"
#include "fff.h"

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_sockets_init);
DECLARE_FAKE_VOID_FUNC(sacn_sockets_deinit);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_validate_netint_config, const SacnMcastNetintId*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_add_receiver_socket, sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                        const SacnMcastNetintId*, size_t, etcpal_socket_t*);
DECLARE_FAKE_VOID_FUNC(sacn_remove_receiver_socket, sacn_thread_id_t, etcpal_socket_t, bool);
DECLARE_FAKE_VOID_FUNC(sacn_add_pending_sockets, SacnRecvThreadContext*);
DECLARE_FAKE_VOID_FUNC(sacn_cleanup_dead_sockets, SacnRecvThreadContext*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_read, SacnRecvThreadContext*, SacnReadResult*);

void sacn_sockets_reset_all_fakes(void);

#endif /* SACN_MOCK_PRIVATE_SOCKETS_H_ */
