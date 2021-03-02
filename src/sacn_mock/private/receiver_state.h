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

#ifndef SACN_MOCK_PRIVATE_RECEIVER_STATE_H_
#define SACN_MOCK_PRIVATE_RECEIVER_STATE_H_

#include "sacn/private/receiver_state.h"
#include "fff.h"

#ifdef __cplusplus
extern "C" {
#endif

// DECLARE_FAKE_VALUE_FUNC...
// DECLARE_FAKE_VOID_FUNC...

void sacn_receiver_state_reset_all_fakes(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_MOCK_PRIVATE_RECEIVER_STATE_H_ */
