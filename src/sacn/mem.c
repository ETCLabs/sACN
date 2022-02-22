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

#include "sacn/private/mem.h"

#include <stddef.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "sacn/private/util.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/*************************** Function definitions ****************************/

/*
 * Memory module initialization and deinitialization (clean up allocated memory) functions.
 */
#if SACN_RECEIVER_ENABLED
etcpal_error_t sacn_receiver_mem_init(unsigned int number_of_threads)
{
#if !SACN_DYNAMIC_MEM
  if (number_of_threads > SACN_RECEIVER_MAX_THREADS)
    return kEtcPalErrNoMem;
#endif
  sacn_mem_set_num_threads(number_of_threads);

  etcpal_error_t res = kEtcPalErrOk;

  if (res == kEtcPalErrOk)
    res = init_recv_thread_context_buf(number_of_threads);
#if SACN_DYNAMIC_MEM
  res = init_status_lists_buf(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_to_erase_bufs(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_universe_data_buf(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_sources_lost_bufs(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_source_pap_lost_buf(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_sampling_started_bufs(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_sampling_ended_bufs(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_source_limit_exceeded_buf(number_of_threads);
#endif  // SACN_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    res = init_remote_sources();
  if (res == kEtcPalErrOk)
    res = init_tracked_sources();
  if (res == kEtcPalErrOk)
    res = init_receivers();

  // Clean up
  if (res != kEtcPalErrOk)
    sacn_receiver_mem_deinit();

  return res;
}

void sacn_receiver_mem_deinit(void)
{
  deinit_receivers();
  deinit_remote_sources();
#if SACN_DYNAMIC_MEM
  deinit_source_limit_exceeded_buf();
  deinit_sampling_ended_bufs();
  deinit_sampling_started_bufs();
  deinit_source_pap_lost_buf();
  deinit_sources_lost_bufs();
  deinit_universe_data_buf();
  deinit_to_erase_bufs();
  deinit_status_lists_buf();
#endif  // SACN_DYNAMIC_MEM
  deinit_recv_thread_context_buf();
}
#endif  // SACN_RECEIVER_ENABLED

#if SACN_SOURCE_ENABLED
etcpal_error_t sacn_source_mem_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (res == kEtcPalErrOk)
    res = init_sources();

  // Clean up
  if (res != kEtcPalErrOk)
    sacn_source_mem_deinit();

  return res;
}

void sacn_source_mem_deinit(void)
{
  deinit_sources();
}
#endif  // SACN_SOURCE_ENABLED

#if SACN_SOURCE_DETECTOR_ENABLED
etcpal_error_t sacn_source_detector_mem_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (res == kEtcPalErrOk)
    res = init_universe_discovery_sources();
  if (res == kEtcPalErrOk)
    res = init_source_detector();

  // Clean up
  if (res != kEtcPalErrOk)
    sacn_source_detector_mem_deinit();

  return res;
}

void sacn_source_detector_mem_deinit(void)
{
  deinit_source_detector();
  deinit_universe_discovery_sources();
}
#endif  // SACN_SOURCE_DETECTOR_ENABLED

#if SACN_MERGE_RECEIVER_ENABLED
etcpal_error_t sacn_merge_receiver_mem_init(unsigned int number_of_threads)
{
#if !SACN_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(number_of_threads);
#endif

  etcpal_error_t res = kEtcPalErrOk;

  if (res == kEtcPalErrOk)
    res = init_merge_receiver_sources();
  if (res == kEtcPalErrOk)
    res = init_merge_receivers();
#if SACN_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    res = init_merged_data_buf(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_non_dmx_buf(number_of_threads);
  if (res == kEtcPalErrOk)
    res = init_merge_receiver_source_limit_exceeded_buf(number_of_threads);
#endif

  // Clean up
  if (res != kEtcPalErrOk)
    sacn_merge_receiver_mem_deinit();

  return res;
}

void sacn_merge_receiver_mem_deinit(void)
{
#if SACN_DYNAMIC_MEM
  deinit_merge_receiver_source_limit_exceeded_buf();
  deinit_non_dmx_buf();
  deinit_merged_data_buf();
#endif
  deinit_merge_receivers();
}
#endif  // SACN_MERGE_RECEIVER_ENABLED
