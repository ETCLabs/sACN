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

#ifndef SACN_PRIVATE_MEM_H_
#define SACN_PRIVATE_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "etcpal/uuid.h"
#include "sacn/receiver.h"
#include "sacn/private/common.h"
#include "sacn/private/sockets.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_mem_init(unsigned int num_threads);
void sacn_mem_deinit(void);

unsigned int sacn_mem_get_num_threads(void);

SacnSourceStatusLists* get_status_lists(sacn_thread_id_t thread_id);
SacnTrackedSource** get_to_erase_buffer(sacn_thread_id_t thread_id, size_t size);
SacnRecvThreadContext* get_recv_thread_context(sacn_thread_id_t thread_id);

// These are processed from the context of receiving data, so there is only one per thread.
UniverseDataNotification* get_universe_data(sacn_thread_id_t thread_id);
SourcePapLostNotification* get_source_pap_lost(sacn_thread_id_t thread_id);
SourceLimitExceededNotification* get_source_limit_exceeded(sacn_thread_id_t thread_id);

// These are processed in the periodic timeout processing, so there are multiple per thread.
SourcesLostNotification* get_sources_lost_buffer(sacn_thread_id_t thread_id, size_t size);
SamplingStartedNotification* get_sampling_started_buffer(sacn_thread_id_t thread_id, size_t size);
SamplingEndedNotification* get_sampling_ended_buffer(sacn_thread_id_t thread_id, size_t size);

bool add_offline_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name, bool terminated);
bool add_online_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name);
bool add_unknown_source(SacnSourceStatusLists* status_lists, const EtcPalUuid* cid, const char* name);

bool add_lost_source(SourcesLostNotification* sources_lost, const EtcPalUuid* cid, const char* name, bool terminated);

bool add_dead_socket(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket);
bool add_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket, etcpal_iptype_t ip_type,
                    bool bound);
bool remove_socket_ref(SacnRecvThreadContext* recv_thread_context, etcpal_socket_t socket);
void add_receiver_to_list(SacnRecvThreadContext* recv_thread_context, SacnReceiver* receiver);
void remove_receiver_from_list(SacnRecvThreadContext* recv_thread_context, SacnReceiver* receiver);

SacnSource* add_sacn_source(sacn_source_t handle);
SacnSourceUniverse* add_sacn_source_universe(SacnSource* source, uint16_t universe);
SacnUnicastDestination* add_sacn_unicast_dest(SacnSourceUniverse* universe, const EtcPalIpAddr* addr);
SacnSourceNetint* add_sacn_source_netint(SacnSource* source, const EtcPalMcastNetintId* id);
etcpal_error_t lookup_source_state(sacn_source_t source, uint16_t universe, SacnSource** source_state,
                                   SacnSourceUniverse** universe_state);
etcpal_error_t lookup_unicast_dest(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* addr,
                                   SacnUnicastDestination** unicast_dest);
SacnSourceNetint* lookup_source_netint(SacnSource* source, const EtcPalMcastNetintId* id);
EtcPalRbTree* get_sacn_sources();
void remove_sacn_source_netint(SacnSource* source, SacnSourceNetint** netint);
void remove_sacn_unicast_dest(SacnSourceUniverse* universe, SacnUnicastDestination** dest, EtcPalRbIter* unicast_iter);
void remove_sacn_source_universe(SacnSource* source, SacnSourceUniverse** universe, EtcPalRbIter* universe_iter);
void remove_sacn_source(SacnSource** source, EtcPalRbIter* source_iter);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MEM_H_ */
