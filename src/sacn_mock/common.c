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

#include "sacn_mock/private/common.h"

#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"

DEFINE_FAKE_VALUE_FUNC(bool, sacn_initialized);
DEFINE_FAKE_VALUE_FUNC(bool, sacn_lock);
DEFINE_FAKE_VOID_FUNC(sacn_unlock);

void sacn_common_reset_all_fakes(void)
{
  RESET_FAKE(sacn_initialized);
  RESET_FAKE(sacn_lock);
  RESET_FAKE(sacn_unlock);

  sacn_initialized_fake.return_val = true;
  sacn_lock_fake.return_val = true;
}
