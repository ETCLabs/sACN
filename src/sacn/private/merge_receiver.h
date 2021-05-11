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

/**
 * @file sacn/private/merge_receiver.h
 * @brief Private constants, types, and function declarations for the
 *        @ref sacn_merge_receiver "sACN Merge Receiver" module.
 */

#ifndef SACN_PRIVATE_MERGE_RECEIVER_H_
#define SACN_PRIVATE_MERGE_RECEIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "sacn/merge_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_merge_receiver_init(void);
void sacn_merge_receiver_deinit(void);

// Receiver callbacks
void merge_receiver_universe_data(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                  const SacnHeaderData* header, const uint8_t* pdata, bool is_sampling, void* context);
void merge_receiver_sources_lost(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                                 size_t num_lost_sources, void* context);
void merge_receiver_sampling_started(sacn_receiver_t handle, uint16_t universe, void* context);
void merge_receiver_sampling_ended(sacn_receiver_t handle, uint16_t universe, void* context);
void merge_receiver_pap_lost(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, void* context);
void merge_receiver_source_limit_exceeded(sacn_receiver_t handle, uint16_t universe, void* context);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MERGE_RECEIVER_H_ */
