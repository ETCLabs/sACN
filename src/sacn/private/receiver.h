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

etcpal_error_t sacn_receiver_init(void);
void sacn_receiver_deinit(void);

etcpal_error_t create_sacn_receiver(const SacnReceiverConfig* config, sacn_receiver_t* handle,
                                    const SacnNetintConfig* netint_config,
                                    const SacnReceiverInternalCallbacks* internal_callbacks);
etcpal_error_t destroy_sacn_receiver(sacn_receiver_t handle);
etcpal_error_t change_sacn_receiver_universe(sacn_receiver_t handle, uint16_t new_universe_id);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_RECEIVER_H_ */
