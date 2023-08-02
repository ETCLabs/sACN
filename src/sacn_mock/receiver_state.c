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

#include "sacn_mock/private/receiver_state.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_state_init);
DEFINE_FAKE_VOID_FUNC(sacn_receiver_state_deinit);
DEFINE_FAKE_VALUE_FUNC(sacn_receiver_t, get_next_receiver_handle);
DEFINE_FAKE_VALUE_FUNC(size_t, get_receiver_netints, const SacnReceiver*, EtcPalMcastNetintId*, size_t);
DEFINE_FAKE_VOID_FUNC(set_expired_wait, uint32_t);
DEFINE_FAKE_VALUE_FUNC(uint32_t, get_expired_wait);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, clear_term_sets_and_sources, SacnReceiver*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, assign_receiver_to_thread, SacnReceiver*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, assign_source_detector_to_thread, SacnSourceDetector*);
DEFINE_FAKE_VOID_FUNC(remove_receiver_from_thread, SacnReceiver*);
DEFINE_FAKE_VOID_FUNC(remove_source_detector_from_thread, SacnSourceDetector*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, add_receiver_sockets, SacnReceiver*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, add_source_detector_sockets, SacnSourceDetector*);
DEFINE_FAKE_VOID_FUNC(begin_sampling_period, SacnReceiver*);
DEFINE_FAKE_VOID_FUNC(remove_receiver_sockets, SacnReceiver*, socket_cleanup_behavior_t);
DEFINE_FAKE_VOID_FUNC(remove_source_detector_sockets, SacnSourceDetector*, socket_cleanup_behavior_t);
DEFINE_FAKE_VOID_FUNC(remove_all_receiver_sockets, socket_cleanup_behavior_t);
DEFINE_FAKE_VOID_FUNC(read_network_and_process, SacnRecvThreadContext*);
DEFINE_FAKE_VOID_FUNC(terminate_sources_on_removed_netints, SacnReceiver*);

void sacn_receiver_state_reset_all_fakes(void)
{
  RESET_FAKE(sacn_receiver_state_init);
  RESET_FAKE(sacn_receiver_state_deinit);
  RESET_FAKE(get_next_receiver_handle);
  RESET_FAKE(get_receiver_netints);
  RESET_FAKE(set_expired_wait);
  RESET_FAKE(get_expired_wait);
  RESET_FAKE(clear_term_sets_and_sources);
  RESET_FAKE(assign_receiver_to_thread);
  RESET_FAKE(assign_source_detector_to_thread);
  RESET_FAKE(remove_receiver_from_thread);
  RESET_FAKE(remove_source_detector_from_thread);
  RESET_FAKE(add_receiver_sockets);
  RESET_FAKE(add_source_detector_sockets);
  RESET_FAKE(begin_sampling_period);
  RESET_FAKE(remove_receiver_sockets);
  RESET_FAKE(remove_source_detector_sockets);
  RESET_FAKE(remove_all_receiver_sockets);
  RESET_FAKE(read_network_and_process);
  RESET_FAKE(terminate_sources_on_removed_netints);
}
