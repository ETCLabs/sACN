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

#ifndef SACN_PRIVATE_SAMPLING_PERIOD_NETINT_MEM_H_
#define SACN_PRIVATE_SAMPLING_PERIOD_NETINT_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_sampling_period_netints(void);

etcpal_error_t add_sacn_sampling_period_netint(EtcPalRbTree* tree, const EtcPalMcastNetintId* id,
                                               bool in_future_sampling_period);
void remove_current_sampling_period_netints(EtcPalRbTree* tree);
etcpal_error_t remove_sampling_period_netint(EtcPalRbTree* tree, const EtcPalMcastNetintId* id);
int sampling_period_netint_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);

void sampling_period_netint_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

void sampling_period_netint_node_dealloc(EtcPalRbNode* node);
EtcPalRbNode* sampling_period_netint_node_alloc(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SAMPLING_PERIOD_NETINT_MEM_H_ */
