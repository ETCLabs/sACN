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

// The maximums set here are based on the following design points:
// - TOTAL_MAX_SOURCES counts the same remote source on multiple universes as multiple sources.
// - Therefore, each source counted in this total only ends up in one universe/receiver.
// - Each source also ends up in only one termination set. Therefore, MAX_TERM_SET_SOURCES = TOTAL_MAX_SOURCES.
// - There can be up to a termination set for each source. Therefore, MAX_TERM_SETS = MAX_TERM_SET_SOURCES.
// - Each source goes into two rbtrees. Nothing else needs rbtrees. Therefore, MAX_RB_NODES = MAX_TERM_SET_SOURCES * 2.
#define SACN_MAX_TERM_SET_SOURCES (SACN_RECEIVER_TOTAL_MAX_SOURCES)
#define SACN_MAX_TERM_SETS SACN_MAX_TERM_SET_SOURCES
#define SACN_SOURCE_LOSS_MAX_RB_NODES (SACN_MAX_TERM_SET_SOURCES * 2)

etcpal_error_t sacn_source_loss_init(void);
void sacn_source_loss_deinit(void);

void mark_sources_online(uint16_t universe, const SacnRemoteSourceInternal* online_sources, size_t num_online_sources,
                         TerminationSet** term_set_list);
etcpal_error_t mark_sources_offline(uint16_t universe, const SacnLostSourceInternal* offline_sources,
                                    size_t num_offline_sources, const SacnRemoteSourceInternal* unknown_sources,
                                    size_t num_unknown_sources, TerminationSet** term_set_list, uint32_t expired_wait);
void get_expired_sources(TerminationSet** term_set_list, SourcesLostNotification* sources_lost);

void clear_term_set_list(TerminationSet* list);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SOURCE_LOSS_H_ */
