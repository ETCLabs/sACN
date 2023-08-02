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

#include "sacn/private/mem/source_detector/source_detector.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/sockets.h"
#include "sacn/private/mem/common.h"
#include "sacn/private/mem/receiver/receiver.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_SOURCE_DETECTOR_ENABLED

/**************************** Private variables ******************************/

static SacnSourceDetector source_detector;

/*************************** Function definitions ****************************/

etcpal_error_t add_sacn_source_detector(const SacnSourceDetectorConfig* config, const SacnNetintConfig* netint_config,
                                        SacnSourceDetector** detector_state)
{
  if (!SACN_ASSERT_VERIFY(config) || !SACN_ASSERT_VERIFY(detector_state))
    return kEtcPalErrSys;

  if (!SACN_ASSERT_VERIFY(config))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

  if (source_detector.created)
    res = kEtcPalErrExists;

  if (res == kEtcPalErrOk)
  {
    source_detector.thread_id = SACN_THREAD_ID_INVALID;

#if SACN_RECEIVER_SOCKET_PER_NIC
#if SACN_DYNAMIC_MEM
    source_detector.sockets.ipv4_sockets = NULL;
    source_detector.sockets.ipv6_sockets = NULL;
#endif  // SACN_DYNAMIC_MEM

    source_detector.sockets.num_ipv4_sockets = 0;
    source_detector.sockets.num_ipv6_sockets = 0;
#else   // SACN_RECEIVER_SOCKET_PER_NIC
    source_detector.sockets.ipv4_socket = ETCPAL_SOCKET_INVALID;
    source_detector.sockets.ipv6_socket = ETCPAL_SOCKET_INVALID;
#endif  // SACN_RECEIVER_SOCKET_PER_NIC

    res = sacn_initialize_source_detector_netints(&source_detector.netints, netint_config);
  }

  if (res == kEtcPalErrOk)
  {
    source_detector.suppress_source_limit_exceeded_notification = false;

    source_detector.callbacks = config->callbacks;
    source_detector.source_count_max = config->source_count_max;
    source_detector.universes_per_source_max = config->universes_per_source_max;
    source_detector.ip_supported = config->ip_supported;

    source_detector.created = true;

    *detector_state = &source_detector;
  }

  return res;
}

SacnSourceDetector* get_sacn_source_detector()
{
  return source_detector.created ? &source_detector : NULL;
}

void remove_sacn_source_detector()
{
  if (source_detector.created)
  {
#if SACN_DYNAMIC_MEM
#if SACN_RECEIVER_SOCKET_PER_NIC
    free(source_detector.sockets.ipv4_sockets);
    free(source_detector.sockets.ipv6_sockets);
#endif

    free(source_detector.netints.netints);
#endif
    source_detector.created = false;
  }
}

etcpal_error_t init_source_detector(void)
{
  source_detector.created = false;

#if SACN_DYNAMIC_MEM
  source_detector.netints.netints = NULL;
  source_detector.netints.netints_capacity = 0;
#endif  // SACN_DYNAMIC_MEM
  source_detector.netints.num_netints = 0;

  return kEtcPalErrOk;
}

void deinit_source_detector(void)
{
  CLEAR_BUF(&source_detector.netints, netints);
}

#endif  // SACN_SOURCE_DETECTOR_ENABLED
