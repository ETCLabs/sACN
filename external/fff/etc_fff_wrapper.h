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
 *******************************************************************************
 * This file is a part of sACN. For more information, go to:
 * https://github.com/ETCLabs/sACN
 ******************************************************************************/

// An ETC wrappper around FFF that provides new definitions of some macros which suppress
// clang-tidy lint warnings.

#ifndef ETC_FFF_WRAPPER_H_
#define ETC_FFF_WRAPPER_H_

#include "fff.h"

#define ETC_FAKE_VALUE_FUNC(...) FAKE_VALUE_FUNC(__VA_ARGS__)  // NOLINT
#define ETC_FAKE_VOID_FUNC(...)  FAKE_VOID_FUNC(__VA_ARGS__)   // NOLINT

#endif  // ETC_FFF_WRAPPER_H_
