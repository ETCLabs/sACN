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

#ifndef SACN_PRIVATE_SOURCE_DETECTOR_STATE_H_
#define SACN_PRIVATE_SOURCE_DETECTOR_STATE_H_

#include "sacn/private/common.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_source_detector_state_init(void);
void sacn_source_detector_state_deinit(void);

size_t get_source_detector_netints(const SacnSourceDetector* detector, EtcPalMcastNetintId* netints,
                                   size_t netints_size);

void handle_sacn_universe_discovery_packet(SacnRecvThreadContext* context, const uint8_t* data, size_t datalen,
                                           const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr,
                                           const char* source_name);
void process_source_detector(SacnRecvThreadContext* recv_thread_context);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SOURCE_DETECTOR_STATE_H_ */
