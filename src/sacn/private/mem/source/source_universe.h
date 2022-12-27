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

#ifndef SACN_PRIVATE_SOURCE_UNIVERSE_MEM_H_
#define SACN_PRIVATE_SOURCE_UNIVERSE_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_source_universes(void);

etcpal_error_t add_sacn_source_universe(SacnSource* source, const SacnSourceUniverseConfig* config,
                                        const SacnNetintConfig* netint_config, SacnSourceUniverse** universe_state);
etcpal_error_t lookup_source_and_universe(sacn_source_t source, uint16_t universe, SacnSource** source_state,
                                          SacnSourceUniverse** universe_state);
size_t get_num_source_universes(SacnSource* source);
etcpal_error_t lookup_universe(SacnSource* source, uint16_t universe, SacnSourceUniverse** universe_state);
void remove_sacn_source_universe_from_tree(SacnSource* source, SacnSourceUniverse* universe);

int source_universe_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
EtcPalRbNode* source_universe_node_alloc(void);
void source_universe_node_dealloc(EtcPalRbNode* node);

void source_universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SOURCE_UNIVERSE_MEM_H_ */
