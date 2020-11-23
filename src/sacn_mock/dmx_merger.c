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

#include "sacn_mock/private/dmx_merger.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_init);
DEFINE_FAKE_VOID_FUNC(sacn_dmx_merger_deinit);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_create, const SacnDmxMergerConfig*, sacn_dmx_merger_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_destroy, sacn_dmx_merger_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_add_source, sacn_dmx_merger_t, sacn_source_id_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_remove_source, sacn_dmx_merger_t, sacn_source_id_t);
DEFINE_FAKE_VALUE_FUNC(sacn_source_id_t, sacn_dmx_merger_get_id, sacn_dmx_merger_t, const EtcPalUuid*);
DEFINE_FAKE_VALUE_FUNC(const SacnDmxMergerSource*, sacn_dmx_merger_get_source, sacn_dmx_merger_t, sacn_source_id_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_update_source_data, sacn_dmx_merger_t, sacn_source_id_t, uint8_t,
                       const uint8_t*, size_t, const uint8_t*, size_t);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_update_source_from_sacn, sacn_dmx_merger_t,
                       const SacnHeaderData*, const uint8_t*);
DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_stop_source_per_address_priority, sacn_dmx_merger_t,
                       sacn_source_id_t);

void sacn_dmx_merger_reset_all_fakes(void)
{
  RESET_FAKE(sacn_dmx_merger_init);
  RESET_FAKE(sacn_dmx_merger_deinit);
  RESET_FAKE(sacn_dmx_merger_create);
  RESET_FAKE(sacn_dmx_merger_destroy);
  RESET_FAKE(sacn_dmx_merger_add_source);
  RESET_FAKE(sacn_dmx_merger_remove_source);
  RESET_FAKE(sacn_dmx_merger_get_id);
  RESET_FAKE(sacn_dmx_merger_get_source);
  RESET_FAKE(sacn_dmx_merger_update_source_data);
  RESET_FAKE(sacn_dmx_merger_update_source_from_sacn);
  RESET_FAKE(sacn_dmx_merger_stop_source_per_address_priority);

  sacn_dmx_merger_init_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_create_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_destroy_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_add_source_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_remove_source_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_get_id_fake.return_val = SACN_DMX_MERGER_SOURCE_INVALID;
  sacn_dmx_merger_get_source_fake.return_val = NULL;
  sacn_dmx_merger_update_source_data_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_update_source_from_sacn_fake.return_val = kEtcPalErrOk;
  sacn_dmx_merger_stop_source_per_address_priority_fake.return_val = kEtcPalErrOk;
}
