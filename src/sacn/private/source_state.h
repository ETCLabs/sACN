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

#ifndef SACN_PRIVATE_SOURCE_STATE_H_
#define SACN_PRIVATE_SOURCE_STATE_H_

#include "sacn/private/common.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_source_state_init(void);
void sacn_source_state_deinit(void);

int take_lock_and_process_sources(bool process_manual);
etcpal_error_t initialize_source_thread();
sacn_source_t get_next_source_handle();
void update_levels_and_or_paps(SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_values,
                               size_t new_values_size, const uint8_t* new_priorities, size_t new_priorities_size,
                               bool force_sync);
void increment_sequence_number(SacnSourceUniverse* universe);
void send_universe_unicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf);
void send_universe_multicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf);
void update_send_buf(uint8_t* send_buf, const uint8_t* new_data, uint16_t new_data_size, bool force_sync);
void set_preview_flag(const SacnSource* source, SacnSourceUniverse* universe, bool preview);
void set_universe_priority(const SacnSource* source, SacnSourceUniverse* universe, uint8_t priority);
void set_unicast_dest_terminating(SacnUnicastDestination* dest);
void reset_transmission_suppression(const SacnSource* source, SacnSourceUniverse* universe, bool reset_null,
                                    bool reset_pap);
void set_universe_terminating(SacnSourceUniverse* universe);
etcpal_error_t add_to_source_netints(SacnSource* source, const EtcPalMcastNetintId* id);
void set_source_terminating(SacnSource* source);
void set_source_name(SacnSource* source, const char* new_name);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SOURCE_STATE_H_ */
