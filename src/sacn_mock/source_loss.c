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

#include "sacn_mock/private/source_loss.h"

DEFINE_FAKE_VALUE_FUNC(etcpal_error_t, sacn_source_loss_init);
DEFINE_FAKE_VOID_FUNC(sacn_source_loss_deinit);
DEFINE_FAKE_VOID_FUNC(mark_sources_online, const SacnRemoteSourceInternal*, size_t, TerminationSet*);
DEFINE_FAKE_VOID_FUNC(mark_sources_offline, const SacnLostSourceInternal*, size_t, const SacnRemoteSourceInternal*,
                      size_t, TerminationSet**, uint32_t);
DEFINE_FAKE_VOID_FUNC(get_expired_sources, TerminationSet**, SourcesLostNotification*);
DEFINE_FAKE_VOID_FUNC(clear_term_set_list, TerminationSet*);

void sacn_source_loss_reset_all_fakes(void)
{
  RESET_FAKE(sacn_source_loss_init);
  RESET_FAKE(sacn_source_loss_deinit);
  RESET_FAKE(mark_sources_online);
  RESET_FAKE(mark_sources_offline);
  RESET_FAKE(get_expired_sources);
  RESET_FAKE(clear_term_set_list);
}
