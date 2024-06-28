/******************************************************************************
 * Copyright 2024 ETC Inc.
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

#ifndef SACN_PRIVATE_SAMPLING_ENDED_MEM_H_
#define SACN_PRIVATE_SAMPLING_ENDED_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "sacn/private/common.h"
#include "sacn/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t init_sampling_ended_bufs(unsigned int num_threads);
void deinit_sampling_ended_bufs(void);

// This is processed in the periodic timeout processing, so there are multiple per thread.
SamplingEndedNotification* get_sampling_ended_buffer(sacn_thread_id_t thread_id, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_SAMPLING_ENDED_MEM_H_ */
