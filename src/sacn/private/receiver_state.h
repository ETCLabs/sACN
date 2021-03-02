/******************************************************************************
 * Copyright 2021 ETC Inc.
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

#ifndef SACN_PRIVATE_RECEIVER_STATE_H_
#define SACN_PRIVATE_RECEIVER_STATE_H_

#include "sacn/private/common.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_receiver_state_init(void);
void sacn_receiver_state_deinit(void);

sacn_receiver_t get_next_receiver_handle();
etcpal_error_t clear_term_sets_and_sources(SacnReceiver* receiver);
etcpal_error_t add_receiver_sockets(SacnReceiver* receiver);
void begin_sampling_period(SacnReceiver* receiver);
void remove_receiver_sockets(SacnReceiver* receiver, socket_close_behavior_t close_behavior);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_RECEIVER_STATE_H_ */
