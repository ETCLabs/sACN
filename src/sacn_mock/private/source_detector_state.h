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

#ifndef SACN_MOCK_PRIVATE_SOURCE_DETECTOR_STATE_H_
#define SACN_MOCK_PRIVATE_SOURCE_DETECTOR_STATE_H_

#include "sacn/private/source_detector_state.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_detector_state_init);
DECLARE_FAKE_VOID_FUNC(sacn_source_detector_state_deinit);
DECLARE_FAKE_VALUE_FUNC(size_t, get_source_detector_netints, const SacnSourceDetector*, EtcPalMcastNetintId*, size_t);
DECLARE_FAKE_VOID_FUNC(handle_sacn_universe_discovery_packet, SacnRecvThreadContext*, const uint8_t*, size_t,
                       const EtcPalUuid*, const EtcPalSockAddr*, const char*);
DECLARE_FAKE_VOID_FUNC(process_source_detector, SacnRecvThreadContext*);

void sacn_source_detector_state_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_SOURCE_DETECTOR_STATE_H_ */
