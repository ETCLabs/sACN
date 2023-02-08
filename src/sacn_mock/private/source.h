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

#ifndef SACN_MOCK_PRIVATE_SOURCE_H_
#define SACN_MOCK_PRIVATE_SOURCE_H_

#include "sacn/private/source.h"
#include "sacn/private/common.h"
#include "sacn/source.h"

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_init);
DECLARE_FAKE_VOID_FUNC(sacn_source_deinit);

DECLARE_FAKE_VOID_FUNC(sacn_source_config_init, SacnSourceConfig*);

DECLARE_FAKE_VOID_FUNC(sacn_source_universe_config_init, SacnSourceUniverseConfig*);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_create, const SacnSourceConfig*, sacn_source_t*);
DECLARE_FAKE_VOID_FUNC(sacn_source_destroy, sacn_source_t);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_name, sacn_source_t, const char*);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_add_universe, sacn_source_t, const SacnSourceUniverseConfig*,
                        const SacnNetintConfig*);
DECLARE_FAKE_VOID_FUNC(sacn_source_remove_universe, sacn_source_t, uint16_t);
DECLARE_FAKE_VALUE_FUNC(size_t, sacn_source_get_universes, sacn_source_t, uint16_t*, size_t);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_add_unicast_destination, sacn_source_t, uint16_t,
                        const EtcPalIpAddr*);
DECLARE_FAKE_VOID_FUNC(sacn_source_remove_unicast_destination, sacn_source_t, uint16_t, const EtcPalIpAddr*);
DECLARE_FAKE_VALUE_FUNC(size_t, sacn_source_get_unicast_destinations, sacn_source_t, uint16_t, EtcPalIpAddr*, size_t);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_priority, sacn_source_t, uint16_t, uint8_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_preview_flag, sacn_source_t, uint16_t, bool);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_synchronization_universe, sacn_source_t, uint16_t, uint16_t);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_send_now, sacn_source_t, uint16_t, uint8_t, const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_send_synchronization, sacn_source_t, uint16_t);

DECLARE_FAKE_VOID_FUNC(sacn_source_update_levels, sacn_source_t, uint16_t, const uint8_t*, size_t);
DECLARE_FAKE_VOID_FUNC(sacn_source_update_levels_and_pap, sacn_source_t, uint16_t, const uint8_t*, size_t,
                       const uint8_t*, size_t);
DECLARE_FAKE_VOID_FUNC(sacn_source_update_levels_and_force_sync, sacn_source_t, uint16_t, const uint8_t*, size_t);
DECLARE_FAKE_VOID_FUNC(sacn_source_update_levels_and_pap_and_force_sync, sacn_source_t, uint16_t, const uint8_t*,
                       size_t, const uint8_t*, size_t);

DECLARE_FAKE_VALUE_FUNC(int, sacn_source_process_manual, sacn_source_tick_mode_t);

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_reset_networking, const SacnNetintConfig*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_reset_networking_per_universe, const SacnNetintConfig*,
                        const SacnSourceUniverseNetintList*, size_t);

DECLARE_FAKE_VALUE_FUNC(size_t, sacn_source_get_network_interfaces, sacn_source_t, uint16_t, EtcPalMcastNetintId*,
                        size_t);

void sacn_source_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_SOURCE_H_ */
