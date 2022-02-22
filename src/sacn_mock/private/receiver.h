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

#ifndef SACN_MOCK_PRIVATE_RECEIVER_H_
#define SACN_MOCK_PRIVATE_RECEIVER_H_

#include "sacn/private/receiver.h"
#include "sacn/private/common.h"
#include "sacn/receiver.h"

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_init);
DECLARE_FAKE_VOID_FUNC(sacn_receiver_deinit);

DECLARE_FAKE_VOID_FUNC(sacn_receiver_config_init, SacnReceiverConfig*);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_create, const SacnReceiverConfig*, sacn_receiver_t*,
                        const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_destroy, sacn_receiver_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_get_universe, sacn_receiver_t, uint16_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_get_footprint, sacn_receiver_t, SacnRecvUniverseSubrange*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_change_universe, sacn_receiver_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_change_footprint, sacn_receiver_t,
                        const SacnRecvUniverseSubrange*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_change_universe_and_footprint, sacn_receiver_t, uint16_t,
                        const SacnRecvUniverseSubrange*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_reset_networking, const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_reset_networking_per_receiver, const SacnNetintConfig*,
                        const SacnReceiverNetintList*, size_t);
DECLARE_FAKE_VALUE_FUNC(size_t, sacn_receiver_get_network_interfaces, sacn_receiver_t, EtcPalMcastNetintId*, size_t);

DECLARE_FAKE_VOID_FUNC(sacn_receiver_set_expired_wait, uint32_t);
DECLARE_FAKE_VALUE_FUNC(uint32_t, sacn_receiver_get_expired_wait);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, create_sacn_receiver, const SacnReceiverConfig*, sacn_receiver_t*,
                        const SacnNetintConfig*, const SacnReceiverInternalCallbacks*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, destroy_sacn_receiver, sacn_receiver_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, change_sacn_receiver_universe, sacn_receiver_t, uint16_t);

void sacn_receiver_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_RECEIVER_H_ */
