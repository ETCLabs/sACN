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

#include "sacn/private/mem/source/source_netint.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/mem/common.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_SOURCE_ENABLED

/*********************** Private function prototypes *************************/

static size_t get_source_netint_index(SacnSource* source, const EtcPalMcastNetintId* id, bool* found);

/*************************** Function definitions ****************************/

// Needs lock
etcpal_error_t add_sacn_source_netint(SacnSource* source, const EtcPalMcastNetintId* id)
{
  SacnSourceNetint* netint = lookup_source_netint(source, id);

  if (netint)
  {
    ++netint->num_refs;
  }
  else
  {
    CHECK_ROOM_FOR_ONE_MORE(source, netints, SacnSourceNetint, SACN_MAX_NETINTS, kEtcPalErrNoMem);

    netint = &source->netints[source->num_netints++];
    netint->id = *id;
    netint->num_refs = 1;
  }

  return kEtcPalErrOk;
}

// Needs lock
SacnSourceNetint* lookup_source_netint(SacnSource* source, const EtcPalMcastNetintId* id)
{
  bool found = false;
  size_t index = get_source_netint_index(source, id, &found);
  return found ? &source->netints[index] : NULL;
}

// Needs lock
SacnSourceNetint* lookup_source_netint_and_index(SacnSource* source, const EtcPalMcastNetintId* id, size_t* index)
{
  bool found = false;
  *index = get_source_netint_index(source, id, &found);
  return found ? &source->netints[*index] : NULL;
}

// Needs lock
void remove_sacn_source_netint(SacnSource* source, size_t index)
{
  REMOVE_AT_INDEX(source, SacnSourceNetint, netints, index);
}

size_t get_source_netint_index(SacnSource* source, const EtcPalMcastNetintId* id, bool* found)
{
  *found = false;
  size_t index = 0;

  while (!(*found) && (index < source->num_netints))
  {
    if ((source->netints[index].id.index == id->index) && (source->netints[index].id.ip_type == id->ip_type))
      *found = true;
    else
      ++index;
  }

  return index;
}

#endif  // SACN_SOURCE_ENABLED
