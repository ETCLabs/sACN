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

#include "sacn_mock/private/source_state.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_state_init);
DEFINE_FAKE_VOID_FUNC(sacn_source_state_deinit);

DEFINE_FAKE_VALUE_FUNC(int, take_lock_and_process_sources, process_sources_behavior_t, sacn_source_tick_mode_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, initialize_source_thread);
DEFINE_FAKE_VALUE_FUNC(sacn_source_t, get_next_source_handle);
DEFINE_FAKE_VOID_FUNC(update_levels_and_or_pap, SacnSource*, SacnSourceUniverse*, const uint8_t*, size_t,
                      const uint8_t*, size_t, force_sync_behavior_t);
DEFINE_FAKE_VOID_FUNC(pack_sequence_number, uint8_t*, uint8_t);
DEFINE_FAKE_VOID_FUNC(increment_sequence_number, SacnSourceUniverse*);
DEFINE_FAKE_VALUE_FUNC(bool, send_universe_unicast, const SacnSource*, SacnSourceUniverse*, const uint8_t*);
DEFINE_FAKE_VALUE_FUNC(bool, send_universe_multicast, const SacnSource*, SacnSourceUniverse*, const uint8_t*);
DEFINE_FAKE_VOID_FUNC(set_preview_flag, const SacnSource*, SacnSourceUniverse*, bool);
DEFINE_FAKE_VOID_FUNC(set_universe_priority, const SacnSource*, SacnSourceUniverse*, uint8_t);
DEFINE_FAKE_VOID_FUNC(set_unicast_dest_terminating, SacnUnicastDestination*, set_terminating_behavior_t);
DEFINE_FAKE_VOID_FUNC(reset_transmission_suppression, const SacnSource*, SacnSourceUniverse*,
                      reset_transmission_suppression_behavior_t);
DEFINE_FAKE_VOID_FUNC(set_universe_terminating, SacnSourceUniverse*, set_terminating_behavior_t);
DEFINE_FAKE_VOID_FUNC(set_source_terminating, SacnSource*);
DEFINE_FAKE_VOID_FUNC(set_source_name, SacnSource*, const char*);
DEFINE_FAKE_VALUE_FUNC(size_t, get_source_universes, const SacnSource*, uint16_t*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, get_source_unicast_dests, const SacnSourceUniverse*, EtcPalIpAddr*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, get_source_universe_netints, const SacnSourceUniverse*, EtcPalMcastNetintId*, size_t);
DEFINE_FAKE_VOID_FUNC(disable_pap_data, SacnSourceUniverse*);
DEFINE_FAKE_VOID_FUNC(clear_source_netints, SacnSource*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, reset_source_universe_networking, SacnSource*, SacnSourceUniverse*,
                       const SacnNetintConfig*);
DEFINE_FAKE_VOID_FUNC(finish_source_universe_termination, SacnSource*, size_t);
DEFINE_FAKE_VOID_FUNC(finish_unicast_dest_termination, SacnSourceUniverse*, size_t);

void sacn_source_state_reset_all_fakes(void)
{
  RESET_FAKE(sacn_source_state_init);
  RESET_FAKE(sacn_source_state_deinit);
  RESET_FAKE(take_lock_and_process_sources);
  RESET_FAKE(initialize_source_thread);
  RESET_FAKE(get_next_source_handle);
  RESET_FAKE(update_levels_and_or_pap);
  RESET_FAKE(pack_sequence_number);
  RESET_FAKE(increment_sequence_number);
  RESET_FAKE(send_universe_unicast);
  RESET_FAKE(send_universe_multicast);
  RESET_FAKE(set_preview_flag);
  RESET_FAKE(set_universe_priority);
  RESET_FAKE(set_unicast_dest_terminating);
  RESET_FAKE(reset_transmission_suppression);
  RESET_FAKE(set_universe_terminating);
  RESET_FAKE(set_source_terminating);
  RESET_FAKE(set_source_name);
  RESET_FAKE(get_source_universes);
  RESET_FAKE(get_source_unicast_dests);
  RESET_FAKE(get_source_universe_netints);
  RESET_FAKE(disable_pap_data);
  RESET_FAKE(clear_source_netints);
  RESET_FAKE(reset_source_universe_networking);
  RESET_FAKE(finish_source_universe_termination);
  RESET_FAKE(finish_unicast_dest_termination);
}
