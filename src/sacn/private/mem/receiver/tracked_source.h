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

#ifndef SACN_PRIVATE_TRACKED_SOURCE_MEM_H_
#define SACN_PRIVATE_TRACKED_SOURCE_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_tracked_sources(void);

etcpal_error_t add_sacn_tracked_source(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const char* name,
                                       const EtcPalMcastNetintId* netint, uint8_t seq_num, uint8_t first_start_code,
                                       SacnTrackedSource** tracked_source_state);
etcpal_error_t clear_receiver_sources(SacnReceiver* receiver);
etcpal_error_t remove_receiver_source(SacnReceiver* receiver, sacn_remote_source_t handle);

void tracked_source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

void tracked_source_node_dealloc(EtcPalRbNode* node);
EtcPalRbNode* tracked_source_node_alloc(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_TRACKED_SOURCE_MEM_H_ */
