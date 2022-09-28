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

#ifndef SACN_PRIVATE_MEM_H_
#define SACN_PRIVATE_MEM_H_

#include <stddef.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "etcpal/uuid.h"
#include "sacn/receiver.h"
#include "sacn/merge_receiver.h"
#include "sacn/source_detector.h"
#include "sacn/private/common.h"
#include "sacn/private/sockets.h"
#include "sacn/private/opts.h"

#include "sacn/private/mem/merge_receiver/merge_receiver.h"
#include "sacn/private/mem/merge_receiver/merge_receiver_source.h"
#include "sacn/private/mem/merge_receiver/merged_data.h"
#include "sacn/private/mem/receiver/receiver.h"
#include "sacn/private/mem/receiver/recv_thread_context.h"
#include "sacn/private/mem/receiver/remote_source.h"
#include "sacn/private/mem/receiver/sampling_ended.h"
#include "sacn/private/mem/receiver/sampling_period_netint.h"
#include "sacn/private/mem/receiver/sampling_started.h"
#include "sacn/private/mem/receiver/source_limit_exceeded.h"
#include "sacn/private/mem/receiver/source_pap_lost.h"
#include "sacn/private/mem/receiver/sources_lost.h"
#include "sacn/private/mem/receiver/status_lists.h"
#include "sacn/private/mem/receiver/to_erase.h"
#include "sacn/private/mem/receiver/tracked_source.h"
#include "sacn/private/mem/receiver/universe_data.h"
#include "sacn/private/mem/source/source.h"
#include "sacn/private/mem/source/source_netint.h"
#include "sacn/private/mem/source/source_universe.h"
#include "sacn/private/mem/source/unicast_destination.h"
#include "sacn/private/mem/source_detector/source_detector.h"
#include "sacn/private/mem/source_detector/source_detector_expired_source.h"
#include "sacn/private/mem/source_detector/universe_discovery_source.h"
#include "sacn/private/mem/common.h"

#ifdef __cplusplus
extern "C" {
#endif

etcpal_error_t sacn_receiver_mem_init(unsigned int number_of_threads);
void sacn_receiver_mem_deinit(void);
etcpal_error_t sacn_source_mem_init(void);
void sacn_source_mem_deinit(void);
etcpal_error_t sacn_source_detector_mem_init(void);
void sacn_source_detector_mem_deinit(void);
etcpal_error_t sacn_merge_receiver_mem_init(unsigned int number_of_threads);
void sacn_merge_receiver_mem_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SACN_PRIVATE_MEM_H_ */
