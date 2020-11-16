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
 * @file sacn/private/universe_discovery.h
 * @brief Private constants, types, and function declarations for the
 *        @ref sacn_universe_discovery "sACN Universe Discovery" module.
 */

#ifndef SACN_PRIVATE_UNIVERSE_DISCOVERY_H_
#define SACN_PRIVATE_UNIVERSE_DISCOVERY_H_

#include <stdbool.h>
#include <stdint.h>
#include "sacn/universe_discovery.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sources have up until the second E1.31 Appendix A E131_E131_UNIVERSE_DISCOVERY_INTERVAL (10s) to report universe
 * changes, and to act as a keep-alive for universe discovery. */
#define DISCOVERY_SOURCE_TIMEOUT_MS 20000

etcpal_error_t sacn_universe_discovery_init(void);
void sacn_universe_discovery_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_UNIVERSE_DISCOVERY_H_ */
