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

#ifndef SACN_MOCK_PRIVATE_DMX_MERGER_H_
#define SACN_MOCK_PRIVATE_DMX_MERGER_H_

#include "sacn/private/dmx_merger.h"

#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_init);
DECLARE_FAKE_VOID_FUNC(sacn_dmx_merger_deinit);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_create, const SacnDmxMergerConfig*, sacn_dmx_merger_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_destroy, sacn_dmx_merger_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_add_source, sacn_dmx_merger_t, sacn_dmx_merger_source_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_remove_source, sacn_dmx_merger_t, sacn_dmx_merger_source_t);
DECLARE_FAKE_VALUE_FUNC(const SacnDmxMergerSource*, sacn_dmx_merger_get_source, sacn_dmx_merger_t,
                        sacn_dmx_merger_source_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_update_levels, sacn_dmx_merger_t, sacn_dmx_merger_source_t,
                        const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_update_pap, sacn_dmx_merger_t, sacn_dmx_merger_source_t,
                        const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_update_universe_priority, sacn_dmx_merger_t,
                        sacn_dmx_merger_source_t, uint8_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_dmx_merger_remove_pap, sacn_dmx_merger_t, sacn_dmx_merger_source_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, create_sacn_dmx_merger, const SacnDmxMergerConfig*, sacn_dmx_merger_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, destroy_sacn_dmx_merger, sacn_dmx_merger_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, remove_sacn_dmx_merger_source, sacn_dmx_merger_t, sacn_dmx_merger_source_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, add_sacn_dmx_merger_source, sacn_dmx_merger_t, sacn_dmx_merger_source_t*);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, add_sacn_dmx_merger_source_with_handle, sacn_dmx_merger_t,
                        sacn_dmx_merger_source_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, update_sacn_dmx_merger_levels, sacn_dmx_merger_t, sacn_dmx_merger_source_t,
                        const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, update_sacn_dmx_merger_pap, sacn_dmx_merger_t, sacn_dmx_merger_source_t,
                        const uint8_t*, size_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, update_sacn_dmx_merger_universe_priority, sacn_dmx_merger_t,
                        sacn_dmx_merger_source_t, uint8_t);
DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, remove_sacn_dmx_merger_pap, sacn_dmx_merger_t, sacn_dmx_merger_source_t);

void sacn_dmx_merger_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_DMX_MERGER_H_ */
