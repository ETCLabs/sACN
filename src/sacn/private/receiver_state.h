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

#ifndef SACN_PRIVATE_RECEIVER_STATE_H_
#define SACN_PRIVATE_RECEIVER_STATE_H_

#include "sacn/private/common.h"

#define SACN_PERIODIC_INTERVAL 120

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_receiver_state_init(void);
void sacn_receiver_state_deinit(void);

sacn_receiver_t get_next_receiver_handle();
size_t get_receiver_netints(const SacnReceiver* receiver, EtcPalMcastNetintId* netints, size_t netints_size);
void set_expired_wait(uint32_t wait_ms);
uint32_t get_expired_wait();
etcpal_error_t clear_term_sets_and_sources(SacnReceiver* receiver);
etcpal_error_t assign_receiver_to_thread(SacnReceiver* receiver);
etcpal_error_t assign_source_detector_to_thread(SacnSourceDetector* detector);
void remove_receiver_from_thread(SacnReceiver* receiver);
void remove_source_detector_from_thread(SacnSourceDetector* detector);
etcpal_error_t add_receiver_sockets(SacnReceiver* receiver);
etcpal_error_t add_source_detector_sockets(SacnSourceDetector* detector);
void begin_sampling_period(SacnReceiver* receiver);
void remove_receiver_sockets(SacnReceiver* receiver, socket_cleanup_behavior_t cleanup_behavior);
void remove_source_detector_sockets(SacnSourceDetector* detector, socket_cleanup_behavior_t cleanup_behavior);
void remove_all_receiver_sockets(socket_cleanup_behavior_t cleanup_behavior);
void read_network_and_process(SacnRecvThreadContext* context);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_RECEIVER_STATE_H_ */
