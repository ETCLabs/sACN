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

#ifndef SACN_MOCK_PRIVATE_RECEIVER_STATE_H_
#define SACN_MOCK_PRIVATE_RECEIVER_STATE_H_

#include "sacn/private/receiver_state.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_state_init);
DECLARE_FAKE_VOID_FUNC(sacn_receiver_state_deinit);
DECLARE_FAKE_VALUE_FUNC(sacn_receiver_t, get_next_receiver_handle);
DECLARE_FAKE_VALUE_FUNC(size_t, get_receiver_netints, const SacnReceiver*, EtcPalMcastNetintId*, size_t);
DECLARE_FAKE_VOID_FUNC(set_expired_wait, uint32_t);
DECLARE_FAKE_VALUE_FUNC(uint32_t, get_expired_wait);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, clear_term_sets_and_sources, SacnReceiver*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, assign_receiver_to_thread, SacnReceiver*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, assign_source_detector_to_thread, SacnSourceDetector*);
DECLARE_FAKE_VOID_FUNC(remove_receiver_from_thread, SacnReceiver*);
DECLARE_FAKE_VOID_FUNC(remove_source_detector_from_thread, SacnSourceDetector*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, add_receiver_sockets, SacnReceiver*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, add_source_detector_sockets, SacnSourceDetector*);
DECLARE_FAKE_VOID_FUNC(begin_sampling_period, SacnReceiver*);
DECLARE_FAKE_VOID_FUNC(remove_receiver_sockets, SacnReceiver*, socket_cleanup_behavior_t);
DECLARE_FAKE_VOID_FUNC(remove_source_detector_sockets, SacnSourceDetector*, socket_cleanup_behavior_t);
DECLARE_FAKE_VOID_FUNC(remove_all_receiver_sockets, socket_cleanup_behavior_t);
DECLARE_FAKE_VOID_FUNC(read_network_and_process, SacnRecvThreadContext*);

void sacn_receiver_state_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_RECEIVER_STATE_H_ */
