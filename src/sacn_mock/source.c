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

#include "sacn_mock/private/source.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_init);
DEFINE_FAKE_VOID_FUNC(sacn_source_deinit);

DEFINE_FAKE_VOID_FUNC(sacn_source_config_init, SacnSourceConfig*);

DEFINE_FAKE_VOID_FUNC(sacn_source_universe_config_init, SacnSourceUniverseConfig*);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_create, const SacnSourceConfig*, sacn_source_t*);
DEFINE_FAKE_VOID_FUNC(sacn_source_destroy, sacn_source_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_name, sacn_source_t, const char*);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_add_universe, sacn_source_t, const SacnSourceUniverseConfig*,
                       const SacnNetintConfig*);
DEFINE_FAKE_VOID_FUNC(sacn_source_remove_universe, sacn_source_t, uint16_t);
DEFINE_FAKE_VALUE_FUNC(size_t, sacn_source_get_universes, sacn_source_t, uint16_t*, size_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_add_unicast_destination, sacn_source_t, uint16_t,
                       const EtcPalIpAddr*);
DEFINE_FAKE_VOID_FUNC(sacn_source_remove_unicast_destination, sacn_source_t, uint16_t, const EtcPalIpAddr*);
DEFINE_FAKE_VALUE_FUNC(size_t, sacn_source_get_unicast_destinations, sacn_source_t, uint16_t, EtcPalIpAddr*, size_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_priority, sacn_source_t, uint16_t, uint8_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_preview_flag, sacn_source_t, uint16_t, bool);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_change_synchronization_universe, sacn_source_t, uint16_t, uint16_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_send_now, sacn_source_t, uint16_t, uint8_t, const uint8_t*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_send_synchronization, sacn_source_t, uint16_t);

DEFINE_FAKE_VOID_FUNC(sacn_source_update_levels, sacn_source_t, uint16_t, const uint8_t*, size_t);
DEFINE_FAKE_VOID_FUNC(sacn_source_update_levels_and_pap, sacn_source_t, uint16_t, const uint8_t*, size_t,
                      const uint8_t*, size_t);
DEFINE_FAKE_VOID_FUNC(sacn_source_update_levels_and_force_sync, sacn_source_t, uint16_t, const uint8_t*, size_t);
DEFINE_FAKE_VOID_FUNC(sacn_source_update_levels_and_pap_and_force_sync, sacn_source_t, uint16_t, const uint8_t*, size_t,
                      const uint8_t*, size_t);

DEFINE_FAKE_VALUE_FUNC(int, sacn_source_process_manual, sacn_source_tick_mode_t);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_reset_networking, const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_reset_networking_per_universe, const SacnNetintConfig*,
                       const SacnSourceUniverseNetintList*, size_t);

DEFINE_FAKE_VALUE_FUNC(size_t, sacn_source_get_network_interfaces, sacn_source_t, uint16_t, EtcPalMcastNetintId*,
                       size_t);

void sacn_source_reset_all_fakes(void)
{
  RESET_FAKE(sacn_source_init);
  RESET_FAKE(sacn_source_deinit);
  RESET_FAKE(sacn_source_config_init);
  RESET_FAKE(sacn_source_universe_config_init);
  RESET_FAKE(sacn_source_create);
  RESET_FAKE(sacn_source_destroy);
  RESET_FAKE(sacn_source_change_name);
  RESET_FAKE(sacn_source_add_universe);
  RESET_FAKE(sacn_source_remove_universe);
  RESET_FAKE(sacn_source_get_universes);
  RESET_FAKE(sacn_source_add_unicast_destination);
  RESET_FAKE(sacn_source_remove_unicast_destination);
  RESET_FAKE(sacn_source_get_unicast_destinations);
  RESET_FAKE(sacn_source_change_priority);
  RESET_FAKE(sacn_source_change_preview_flag);
  RESET_FAKE(sacn_source_change_synchronization_universe);
  RESET_FAKE(sacn_source_send_now);
  RESET_FAKE(sacn_source_send_synchronization);
  RESET_FAKE(sacn_source_update_levels);
  RESET_FAKE(sacn_source_update_levels_and_pap);
  RESET_FAKE(sacn_source_update_levels_and_force_sync);
  RESET_FAKE(sacn_source_update_levels_and_pap_and_force_sync);
  RESET_FAKE(sacn_source_process_manual);
  RESET_FAKE(sacn_source_reset_networking);
  RESET_FAKE(sacn_source_reset_networking_per_universe);
  RESET_FAKE(sacn_source_get_network_interfaces);
}
