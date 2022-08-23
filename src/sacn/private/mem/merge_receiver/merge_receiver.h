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

#ifndef SACN_PRIVATE_MERGE_RECEIVER_MEM_H_
#define SACN_PRIVATE_MERGE_RECEIVER_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_merge_receivers(void);
void deinit_merge_receivers(void);

etcpal_error_t add_sacn_merge_receiver(sacn_merge_receiver_t handle, const SacnMergeReceiverConfig* config,
                                       SacnMergeReceiver** state);
etcpal_error_t lookup_merge_receiver(sacn_merge_receiver_t handle, SacnMergeReceiver** state);
size_t get_num_merge_receivers();
void remove_sacn_merge_receiver(sacn_merge_receiver_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MERGE_RECEIVER_MEM_H_ */
