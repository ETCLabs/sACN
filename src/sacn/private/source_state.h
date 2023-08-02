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

#ifndef SACN_PRIVATE_SOURCE_STATE_H_
#define SACN_PRIVATE_SOURCE_STATE_H_

#include "sacn/private/common.h"

typedef enum
{
  kProcessManualSources,
  kProcessThreadedSources
} process_sources_behavior_t;

typedef enum
{
  kResetLevel,
  kResetPap,
  kResetLevelAndPap
} reset_transmission_suppression_behavior_t;

typedef enum
{
  kIncludeTerminatingUnicastDests,
  kSkipTerminatingUnicastDests
} send_universe_unicast_behavior_t;

typedef enum
{
  kTerminateAndRemove,
  kTerminateWithoutRemoving
} set_terminating_behavior_t;

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_source_state_init(void);
void sacn_source_state_deinit(void);

int take_lock_and_process_sources(process_sources_behavior_t behavior, sacn_source_tick_mode_t tick_mode);
etcpal_error_t initialize_source_thread();
sacn_source_t get_next_source_handle();
void update_levels_and_or_pap(SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_values,
                              size_t new_values_size, const uint8_t* new_priorities, size_t new_priorities_size,
                              force_sync_behavior_t force_sync);
void pack_sequence_number(uint8_t* buf, uint8_t seq_num);
void increment_sequence_number(SacnSourceUniverse* universe);
bool send_universe_unicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf);
bool send_universe_multicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf);
void set_preview_flag(const SacnSource* source, SacnSourceUniverse* universe, bool preview);
void set_universe_priority(const SacnSource* source, SacnSourceUniverse* universe, uint8_t priority);
void set_unicast_dest_terminating(SacnUnicastDestination* dest, set_terminating_behavior_t behavior);
void reset_transmission_suppression(const SacnSource* source, SacnSourceUniverse* universe,
                                    reset_transmission_suppression_behavior_t behavior);
void set_universe_terminating(SacnSourceUniverse* universe, set_terminating_behavior_t behavior);
void set_source_terminating(SacnSource* source);
void set_source_name(SacnSource* source, const char* new_name);
size_t get_source_universes(const SacnSource* source, uint16_t* universes, size_t universes_size);
size_t get_source_unicast_dests(const SacnSourceUniverse* universe, EtcPalIpAddr* destinations,
                                size_t destinations_size);
size_t get_source_universe_netints(const SacnSourceUniverse* universe, EtcPalMcastNetintId* netints,
                                   size_t netints_size);
void disable_pap_data(SacnSourceUniverse* universe);
void clear_source_netints(SacnSource* source);
etcpal_error_t reset_source_universe_networking(SacnSource* source, SacnSourceUniverse* universe,
                                                const SacnNetintConfig* netint_config);
void finish_source_universe_termination(SacnSource* source, size_t index);
void finish_unicast_dest_termination(SacnSourceUniverse* universe, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SOURCE_STATE_H_ */
