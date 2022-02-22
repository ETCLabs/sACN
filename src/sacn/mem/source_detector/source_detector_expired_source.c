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

#include "sacn/private/mem/source_detector/source_detector_expired_source.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"
#include "sacn/private/mem/receiver/remote_source.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_SOURCE_DETECTOR_ENABLED

// Suppress strncpy() warning on Windows/MSVC.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/*************************** Function definitions ****************************/

etcpal_error_t add_sacn_source_detector_expired_source(SourceDetectorSourceExpiredNotification* source_expired,
                                                       sacn_remote_source_t handle, const char* name)
{
  if (!source_expired || (handle == SACN_REMOTE_SOURCE_INVALID) || !name)
    return kEtcPalErrInvalid;

  const EtcPalUuid* cid = get_remote_source_cid(handle);
  if (!cid)
    return kEtcPalErrNotFound;

#if SACN_DYNAMIC_MEM
  if (!source_expired->expired_sources)
  {
    source_expired->expired_sources = calloc(INITIAL_CAPACITY, sizeof(SourceDetectorExpiredSource));
    if (source_expired->expired_sources)
      source_expired->expired_sources_capacity = INITIAL_CAPACITY;
    else
      return kEtcPalErrNoMem;
  }
#endif

  CHECK_ROOM_FOR_ONE_MORE(source_expired, expired_sources, SourceDetectorExpiredSource,
                          SACN_SOURCE_DETECTOR_MAX_SOURCES, kEtcPalErrNoMem);

  source_expired->expired_sources[source_expired->num_expired_sources].handle = handle;
  source_expired->expired_sources[source_expired->num_expired_sources].cid = *cid;
  strncpy(source_expired->expired_sources[source_expired->num_expired_sources].name, name, SACN_SOURCE_NAME_MAX_LEN);
  ++source_expired->num_expired_sources;

  return kEtcPalErrOk;
}

#endif  // SACN_SOURCE_DETECTOR_ENABLED
