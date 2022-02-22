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

#ifndef SACN_PRIVATE_SOURCE_LOSS_H_
#define SACN_PRIVATE_SOURCE_LOSS_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/rbtree.h"
#include "etcpal/timer.h"
#include "etcpal/uuid.h"
#include "sacn/receiver.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SACN_MAX_TERM_SETS SACN_RECEIVER_TOTAL_MAX_SOURCES
#define SACN_MAX_TERM_SET_SOURCES (SACN_RECEIVER_TOTAL_MAX_SOURCES * 2)
#define SACN_SOURCE_LOSS_MAX_RB_NODES (SACN_MAX_TERM_SET_SOURCES)

etcpal_error_t sacn_source_loss_init(void);
void sacn_source_loss_deinit(void);

void mark_sources_online(const SacnRemoteSourceInternal* online_sources, size_t num_online_sources,
                         TerminationSet* term_set_list);
void mark_sources_offline(const SacnLostSourceInternal* offline_sources, size_t num_offline_sources,
                          const SacnRemoteSourceInternal* unknown_sources, size_t num_unknown_sources,
                          TerminationSet** term_set_list, uint32_t expired_wait);
void get_expired_sources(TerminationSet** term_set_list, SourcesLostNotification* sources_lost);

void clear_term_set_list(TerminationSet* list);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SOURCE_LOSS_H_ */
