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

/**
 * @file sacn/private/merge_receiver.h
 * @brief Private constants, types, and function declarations for the
 *        @ref sacn_merge_receiver "sACN Merge Receiver" module.
 */

#ifndef SACN_PRIVATE_MERGE_RECEIVER_H_
#define SACN_PRIVATE_MERGE_RECEIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "sacn/merge_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

//TODO: I put this here so the tests will build.  FLESH THIS OUT..
/* The internal state/glue needed to map a sacn_receiver_t to a sacn_dmx_merger_t, etc. */
typedef struct SacnMergeReceiver
{
  // Configured callbacks
  SacnMergeReceiverCallbacks callbacks;

  struct SacnMergeReceiver* next;
} SacnMergeReceiver;

etcpal_error_t sacn_merge_receiver_init(void);
void sacn_merge_receiver_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MERGE_RECEIVER_H_ */
