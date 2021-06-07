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

#ifndef SACN_PRIVATE_RECV_THREAD_CONTEXT_MEM_H_
#define SACN_PRIVATE_RECV_THREAD_CONTEXT_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_recv_thread_context_buf(unsigned int num_threads);
void deinit_recv_thread_context_buf(void);

SacnRecvThreadContext* get_recv_thread_context(sacn_thread_id_t thread_id);

bool add_dead_socket(SacnRecvThreadContext* context, etcpal_socket_t socket);
bool add_socket_ref(SacnRecvThreadContext* context, etcpal_socket_t socket, etcpal_iptype_t ip_type,
                    bool bound);
bool remove_socket_ref(SacnRecvThreadContext* context, etcpal_socket_t socket);
void add_receiver_to_list(SacnRecvThreadContext* context, SacnReceiver* receiver);
void remove_receiver_from_list(SacnRecvThreadContext* context, SacnReceiver* receiver);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_RECV_THREAD_CONTEXT_MEM_H_ */
