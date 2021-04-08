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

#include "sacn/private/common.h"

#include "sacn/private/source_detector_state.h"

/****************************** Private macros *******************************/

/***************************** Private constants *****************************/

/****************************** Private types ********************************/

/**************************** Private variables ******************************/

/*********************** Private function prototypes *************************/

/*************************** Function definitions ****************************/

etcpal_error_t sacn_source_detector_state_init(void)
{
  // Nothing to do

  return kEtcPalErrOk;
}

void sacn_source_detector_state_deinit(void)
{
  // Nothing to do
}

size_t get_source_detector_netints(const SacnSourceDetector* detector, EtcPalMcastNetintId* netints,
                                   size_t netints_size)
{
  for (size_t i = 0; netints && (i < netints_size) && (i < detector->netints.num_netints); ++i)
    netints[i] = detector->netints.netints[i];

  return detector->netints.num_netints;
}

void handle_sacn_universe_discovery_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                                           const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr,
                                           const char* source_name)
{
  ETCPAL_UNUSED_ARG(thread_id);
  ETCPAL_UNUSED_ARG(data);
  ETCPAL_UNUSED_ARG(datalen);
  ETCPAL_UNUSED_ARG(sender_cid);
  ETCPAL_UNUSED_ARG(from_addr);
  ETCPAL_UNUSED_ARG(source_name);

  // TODO
}
