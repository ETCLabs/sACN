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

#include "sacn/private/mem/source/source_universe.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/sockets.h"
#include "sacn/private/pdu.h"
#include "sacn/private/mem/common.h"
#include "sacn/private/mem/source/source.h"
#include "sacn/private/mem/source/unicast_destination.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_SOURCE_ENABLED

/*************************** Function definitions ****************************/

// Needs lock
etcpal_error_t add_sacn_source_universe(SacnSource* source, const SacnSourceUniverseConfig* config,
                                        const SacnNetintConfig* netint_config, SacnSourceUniverse** universe_state)
{
  etcpal_error_t result = kEtcPalErrOk;

#if SACN_DYNAMIC_MEM
  // Make sure to check against universe_count_max.
  if ((source->universe_count_max != SACN_SOURCE_INFINITE_UNIVERSES) &&
      (source->num_universes >= source->universe_count_max))
  {
    result = kEtcPalErrNoMem;  // No room to allocate additional universe.
  }
#endif

  SacnSourceUniverse* universe = NULL;
  if (result == kEtcPalErrOk)
  {
    CHECK_ROOM_FOR_ONE_MORE(source, universes, SacnSourceUniverse, SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE,
                            kEtcPalErrNoMem);

    universe = &source->universes[source->num_universes];

    universe->universe_id = config->universe;

    universe->termination_state = kNotTerminating;
    universe->num_terminations_sent = 0;

    universe->priority = config->priority;
    universe->sync_universe = config->sync_universe;
    universe->send_preview = config->send_preview;
    universe->seq_num = 0;

    universe->level_packets_sent_before_suppression = 0;
    init_sacn_data_send_buf(universe->level_send_buf, SACN_STARTCODE_DMX, &source->cid, source->name, config->priority,
                            config->universe, config->sync_universe, config->send_preview);
    universe->has_level_data = false;

#if SACN_ETC_PRIORITY_EXTENSION
    universe->pap_packets_sent_before_suppression = 0;
    init_sacn_data_send_buf(universe->pap_send_buf, SACN_STARTCODE_PRIORITY, &source->cid, source->name,
                            config->priority, config->universe, config->sync_universe, config->send_preview);
    universe->has_pap_data = false;
#endif

    universe->send_unicast_only = config->send_unicast_only;

    universe->num_unicast_dests = 0;
#if SACN_DYNAMIC_MEM
    universe->unicast_dests = calloc(INITIAL_CAPACITY, sizeof(SacnUnicastDestination));
    universe->unicast_dests_capacity = universe->unicast_dests ? INITIAL_CAPACITY : 0;

    if (!universe->unicast_dests)
      result = kEtcPalErrNoMem;

    universe->netints.netints = NULL;
    universe->netints.netints_capacity = 0;
#endif
    universe->netints.num_netints = 0;
  }

  for (size_t i = 0; (result == kEtcPalErrOk) && (i < config->num_unicast_destinations); ++i)
  {
    SacnUnicastDestination* dest = NULL;
    result = add_sacn_unicast_dest(universe, &config->unicast_destinations[i], &dest);

    if (result == kEtcPalErrExists)
      result = kEtcPalErrOk;  // Duplicates are automatically filtered and not a failure condition.
  }

  if (result == kEtcPalErrOk)
    result = sacn_initialize_source_netints(&universe->netints, netint_config);

  if (result == kEtcPalErrOk)
  {
    ++source->num_universes;
  }
  else if (universe)
  {
    CLEAR_BUF(&universe->netints, netints);
    CLEAR_BUF(universe, unicast_dests);
  }

  *universe_state = universe;

  return result;
}

// Needs lock
etcpal_error_t lookup_source_and_universe(sacn_source_t source, uint16_t universe, SacnSource** source_state,
                                          SacnSourceUniverse** universe_state)
{
  etcpal_error_t result = lookup_source(source, source_state);

  if (result == kEtcPalErrOk)
    result = lookup_universe(*source_state, universe, universe_state);

  return result;
}

// Needs lock
etcpal_error_t lookup_universe(SacnSource* source, uint16_t universe, SacnSourceUniverse** universe_state)
{
  bool found = false;
  size_t index = get_source_universe_index(source, universe, &found);
  *universe_state = found ? &source->universes[index] : NULL;
  return found ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
void remove_sacn_source_universe(SacnSource* source, size_t index)
{
  CLEAR_BUF(&source->universes[index], unicast_dests);
  CLEAR_BUF(&source->universes[index].netints, netints);
  REMOVE_AT_INDEX(source, SacnSourceUniverse, universes, index);
}

// Needs lock
size_t get_source_universe_index(SacnSource* source, uint16_t universe, bool* found)
{
  *found = false;
  size_t index = 0;

  while (!(*found) && (index < source->num_universes))
  {
    if (source->universes[index].universe_id == universe)
      *found = true;
    else
      ++index;
  }

  return index;
}

#endif  // SACN_SOURCE_ENABLED
