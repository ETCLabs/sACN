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

#ifndef SACN_MOCK_PRIVATE_SOURCE_LOSS_H_
#define SACN_MOCK_PRIVATE_SOURCE_LOSS_H_

#include "sacn/private/source_loss.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_loss_init);
DECLARE_FAKE_VOID_FUNC(sacn_source_loss_deinit);

DECLARE_FAKE_VOID_FUNC(mark_sources_online, const SacnRemoteSourceInternal*, size_t, TerminationSet*);
DECLARE_FAKE_VOID_FUNC(mark_sources_offline, const SacnLostSourceInternal*, size_t, const SacnRemoteSourceInternal*,
                       size_t, TerminationSet**, uint32_t);
DECLARE_FAKE_VOID_FUNC(get_expired_sources, TerminationSet**, SourcesLostNotification*);

DECLARE_FAKE_VOID_FUNC(clear_term_set_list, TerminationSet*);

void sacn_source_loss_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_SOURCE_LOSS_H_ */
