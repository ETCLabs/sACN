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
 * @file sacn/private/receiver.h
 * @brief Private constants, types, and function declarations for the
 *        @ref sacn_receiver "sACN Receiver" module.
 */

#ifndef SACN_PRIVATE_RECEIVER_H_
#define SACN_PRIVATE_RECEIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "sacn/receiver.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SACN_RECEIVER_MAX_RB_NODES ((SACN_RECEIVER_MAX_UNIVERSES * 2) + SACN_RECEIVER_TOTAL_MAX_SOURCES)

/* The data-loss timeout, as defined in E1.31 */
#define DATA_LOSS_TIMEOUT 2500
/* How long to wait for a 0xdd packet once a new source is discovered */
#define WAIT_FOR_PRIORITY 1500
/* Length of the sampling period for a new universe */
#define SAMPLE_TIME 1500

etcpal_error_t sacn_receiver_init(void);
void sacn_receiver_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_RECEIVER_H_ */
