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

#include "sacn_mock/private/receiver.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_init);
DEFINE_FAKE_VOID_FUNC(sacn_receiver_deinit);

DEFINE_FAKE_VOID_FUNC(sacn_receiver_config_init, SacnReceiverConfig*);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_create, const SacnReceiverConfig*, sacn_receiver_t*,
                       const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_destroy, sacn_receiver_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_get_universe, sacn_receiver_t, uint16_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_get_footprint, sacn_receiver_t, SacnRecvUniverseSubrange*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_change_universe, sacn_receiver_t, uint16_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_change_footprint, sacn_receiver_t,
                       const SacnRecvUniverseSubrange*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_change_universe_and_footprint, sacn_receiver_t, uint16_t,
                       const SacnRecvUniverseSubrange*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_reset_networking, const SacnNetintConfig*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_receiver_reset_networking_per_receiver, const SacnNetintConfig*,
                       const SacnReceiverNetintList*, size_t);
DEFINE_FAKE_VALUE_FUNC(size_t, sacn_receiver_get_network_interfaces, sacn_receiver_t, EtcPalMcastNetintId*, size_t);

DEFINE_FAKE_VOID_FUNC(sacn_receiver_set_expired_wait, uint32_t);
DEFINE_FAKE_VALUE_FUNC(uint32_t, sacn_receiver_get_expired_wait);

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, create_sacn_receiver, const SacnReceiverConfig*, sacn_receiver_t*,
                       const SacnNetintConfig*, const SacnReceiverInternalCallbacks*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, destroy_sacn_receiver, sacn_receiver_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, change_sacn_receiver_universe, sacn_receiver_t, uint16_t);

void sacn_receiver_reset_all_fakes(void)
{
  RESET_FAKE(sacn_receiver_init);
  RESET_FAKE(sacn_receiver_deinit);
  RESET_FAKE(sacn_receiver_config_init);
  RESET_FAKE(sacn_receiver_create);
  RESET_FAKE(sacn_receiver_destroy);
  RESET_FAKE(sacn_receiver_get_universe);
  RESET_FAKE(sacn_receiver_get_footprint);
  RESET_FAKE(sacn_receiver_change_universe);
  RESET_FAKE(sacn_receiver_change_footprint);
  RESET_FAKE(sacn_receiver_change_universe_and_footprint);
  RESET_FAKE(sacn_receiver_reset_networking);
  RESET_FAKE(sacn_receiver_reset_networking_per_receiver);
  RESET_FAKE(sacn_receiver_get_network_interfaces);
  RESET_FAKE(sacn_receiver_set_expired_wait);
  RESET_FAKE(sacn_receiver_get_expired_wait);
  RESET_FAKE(create_sacn_receiver);
  RESET_FAKE(destroy_sacn_receiver);
  RESET_FAKE(change_sacn_receiver_universe);
}
