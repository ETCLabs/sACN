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

#ifndef SACN_MOCK_PRIVATE_SOURCE_STATE_H_
#define SACN_MOCK_PRIVATE_SOURCE_STATE_H_

#include "sacn/private/source_state.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_state_init);
DECLARE_FAKE_VOID_FUNC(sacn_source_state_deinit);

DECLARE_FAKE_VALUE_FUNC(int, take_lock_and_process_sources, process_sources_behavior_t, sacn_source_tick_mode_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, initialize_source_thread);
DECLARE_FAKE_VALUE_FUNC(sacn_source_t, get_next_source_handle);
DECLARE_FAKE_VOID_FUNC(update_levels_and_or_pap, SacnSource*, SacnSourceUniverse*, const uint8_t*, size_t,
                       const uint8_t*, size_t, force_sync_behavior_t);
DECLARE_FAKE_VOID_FUNC(pack_sequence_number, uint8_t*, uint8_t);
DECLARE_FAKE_VOID_FUNC(increment_sequence_number, SacnSourceUniverse*);
DECLARE_FAKE_VALUE_FUNC(bool, send_universe_unicast, const SacnSource*, SacnSourceUniverse*, const uint8_t*);
DECLARE_FAKE_VALUE_FUNC(bool, send_universe_multicast, const SacnSource*, SacnSourceUniverse*, const uint8_t*);
DECLARE_FAKE_VOID_FUNC(set_preview_flag, const SacnSource*, SacnSourceUniverse*, bool);
DECLARE_FAKE_VOID_FUNC(set_universe_priority, const SacnSource*, SacnSourceUniverse*, uint8_t);
DECLARE_FAKE_VOID_FUNC(set_unicast_dest_terminating, SacnUnicastDestination*, set_terminating_behavior_t);
DECLARE_FAKE_VOID_FUNC(reset_transmission_suppression, const SacnSource*, SacnSourceUniverse*,
                       reset_transmission_suppression_behavior_t);
DECLARE_FAKE_VOID_FUNC(set_universe_terminating, SacnSourceUniverse*, set_terminating_behavior_t);
DECLARE_FAKE_VOID_FUNC(set_source_terminating, SacnSource*);
DECLARE_FAKE_VOID_FUNC(set_source_name, SacnSource*, const char*);
DECLARE_FAKE_VALUE_FUNC(size_t, get_source_universes, const SacnSource*, uint16_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(size_t, get_source_unicast_dests, const SacnSourceUniverse*, EtcPalIpAddr*, size_t);
DECLARE_FAKE_VALUE_FUNC(size_t, get_source_universe_netints, const SacnSourceUniverse*, EtcPalMcastNetintId*, size_t);
DECLARE_FAKE_VOID_FUNC(disable_pap_data, SacnSourceUniverse*);
DECLARE_FAKE_VOID_FUNC(clear_source_netints, SacnSource*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, reset_source_universe_networking, SacnSource*, SacnSourceUniverse*,
                        const SacnNetintConfig*);
DECLARE_FAKE_VOID_FUNC(finish_source_universe_termination, SacnSource*, size_t);
DECLARE_FAKE_VOID_FUNC(finish_unicast_dest_termination, SacnSourceUniverse*, size_t);

void sacn_source_state_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_SOURCE_STATE_H_ */
