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

#include "sacn/private/mem/source/source.h"

#include <stddef.h>
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn/private/sockets.h"
#include "sacn/private/pdu.h"
#include "sacn/private/mem/common.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_SOURCE_ENABLED

/**************************** Private variables ******************************/

static bool sources_initialized = false;

static struct SacnSourceMem
{
  SACN_DECLARE_SOURCE_BUF(SacnSource, sources, SACN_SOURCE_MAX_SOURCES);
  size_t num_sources;
} sacn_pool_source_mem;

/*********************** Private function prototypes *************************/

static size_t get_source_index(sacn_source_t handle, bool* found);

/*************************** Function definitions ****************************/

// Needs lock
etcpal_error_t add_sacn_source(sacn_source_t handle, const SacnSourceConfig* config, SacnSource** source_state)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_SOURCE_INVALID) || !SACN_ASSERT_VERIFY(config) ||
      !SACN_ASSERT_VERIFY(source_state))
  {
    return kEtcPalErrSys;
  }

  etcpal_error_t result = kEtcPalErrOk;
  SacnSource* source = NULL;
  if (lookup_source(handle, &source) == kEtcPalErrOk)
    result = kEtcPalErrExists;

  if (result == kEtcPalErrOk)
  {
    CHECK_ROOM_FOR_ONE_MORE((&sacn_pool_source_mem), sources, SacnSource, SACN_SOURCE_MAX_SOURCES, kEtcPalErrNoMem);

    source = &sacn_pool_source_mem.sources[sacn_pool_source_mem.num_sources];
  }

  if (result == kEtcPalErrOk)
  {
    source->handle = handle;

    // Initialize the universe discovery send buffer.
    memset(source->universe_discovery_send_buf, 0, SACN_UNIVERSE_DISCOVERY_PACKET_MTU);

    int written = 0;
    written += pack_sacn_root_layer(source->universe_discovery_send_buf, SACN_UNIVERSE_DISCOVERY_HEADER_SIZE, true,
                                    &config->cid);
    written +=
        pack_sacn_universe_discovery_framing_layer(&source->universe_discovery_send_buf[written], 0, config->name);
    written += pack_sacn_universe_discovery_layer_header(&source->universe_discovery_send_buf[written], 0, 0, 0);

    // Initialize everything else.
    source->cid = config->cid;
    memset(source->name, 0, SACN_SOURCE_NAME_MAX_LEN);
    memcpy(source->name, config->name, strlen(config->name));
    source->terminating = false;
    source->num_active_universes = 0;
    etcpal_timer_start(&source->universe_discovery_timer, SACN_UNIVERSE_DISCOVERY_INTERVAL);
    source->process_manually = config->manually_process_source;
    source->ip_supported = config->ip_supported;
    source->keep_alive_interval = config->keep_alive_interval;
    source->pap_keep_alive_interval = config->pap_keep_alive_interval;
    source->universe_count_max = config->universe_count_max;

    etcpal_timer_start(&source->stats_log_timer, SACN_STATS_LOG_INTERVAL);
    source->total_tick_count = 0;
    source->failed_tick_count = 0;

    source->num_universes = 0;
    source->num_netints = 0;
#if SACN_DYNAMIC_MEM
    source->universes = calloc(INITIAL_CAPACITY, sizeof(SacnSourceUniverse));
    source->universes_capacity = source->universes ? INITIAL_CAPACITY : 0;
    source->netints = calloc(INITIAL_CAPACITY, sizeof(SacnSourceNetint));
    source->netints_capacity = source->netints ? INITIAL_CAPACITY : 0;

    if (!source->universes || !source->netints)
      result = kEtcPalErrNoMem;
#else
    memset(source->universes, 0, sizeof(source->universes));
    memset(source->netints, 0, sizeof(source->netints));
#endif
  }

  if (result == kEtcPalErrOk)
  {
    ++sacn_pool_source_mem.num_sources;
  }
  else if (source)
  {
    CLEAR_BUF(source, universes);
    CLEAR_BUF(source, netints);
  }

  *source_state = source;

  return result;
}

// Needs lock
etcpal_error_t lookup_source(sacn_source_t handle, SacnSource** source_state)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_SOURCE_INVALID) || !SACN_ASSERT_VERIFY(source_state))
    return kEtcPalErrSys;

  bool found = false;
  size_t index = get_source_index(handle, &found);
  *source_state = found ? &sacn_pool_source_mem.sources[index] : NULL;
  return found ? kEtcPalErrOk : kEtcPalErrNotFound;
}

SacnSource* get_source(size_t index)
{
  return (index < sacn_pool_source_mem.num_sources) ? &sacn_pool_source_mem.sources[index] : NULL;
}

size_t get_num_sources()
{
  return sacn_pool_source_mem.num_sources;
}

// Needs lock
void remove_sacn_source(size_t index)
{
  CLEAR_BUF(&sacn_pool_source_mem.sources[index], universes);
  CLEAR_BUF(&sacn_pool_source_mem.sources[index], netints);

  REMOVE_AT_INDEX((&sacn_pool_source_mem), SacnSource, sources, index);
}

size_t get_source_index(sacn_source_t handle, bool* found)
{
  if (!SACN_ASSERT_VERIFY(found))
    return 0;

  *found = false;
  size_t index = 0;

  while (!(*found) && (index < sacn_pool_source_mem.num_sources))
  {
    if (sacn_pool_source_mem.sources[index].handle == handle)
      *found = true;
    else
      ++index;
  }

  return index;
}

etcpal_error_t init_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;
#if SACN_DYNAMIC_MEM
  sacn_pool_source_mem.sources = calloc(INITIAL_CAPACITY, sizeof(SacnSource));
  sacn_pool_source_mem.sources_capacity = sacn_pool_source_mem.sources ? INITIAL_CAPACITY : 0;
  if (!sacn_pool_source_mem.sources)
    res = kEtcPalErrNoMem;
#else   // SACN_DYNAMIC_MEM
  memset(sacn_pool_source_mem.sources, 0, sizeof(sacn_pool_source_mem.sources));
#endif  // SACN_DYNAMIC_MEM
  sacn_pool_source_mem.num_sources = 0;

  if (res == kEtcPalErrOk)
    sources_initialized = true;

  return res;
}

// Takes lock
void deinit_sources(void)
{
  if (sacn_lock())
  {
    if (sources_initialized)
    {
      for (size_t i = 0; i < sacn_pool_source_mem.num_sources; ++i)
      {
        for (size_t j = 0; j < sacn_pool_source_mem.sources[i].num_universes; ++j)
        {
          CLEAR_BUF(&sacn_pool_source_mem.sources[i].universes[j].netints, netints);
          CLEAR_BUF(&sacn_pool_source_mem.sources[i].universes[j], unicast_dests);
        }

        CLEAR_BUF(&sacn_pool_source_mem.sources[i], universes);
        CLEAR_BUF(&sacn_pool_source_mem.sources[i], netints);
      }

      CLEAR_BUF(&sacn_pool_source_mem, sources);
#if SACN_DYNAMIC_MEM
      sacn_pool_source_mem.sources_capacity = 0;
#endif

      memset(&sacn_pool_source_mem, 0, sizeof sacn_pool_source_mem);

      sources_initialized = false;
    }

    sacn_unlock();
  }
}

#endif  // SACN_SOURCE_ENABLED
