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

#include "sacn/private/mem/source_detector/universe_discovery_source.h"

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

/**************************** Private constants ******************************/

#define SACN_UNIVERSE_DISCOVERY_SOURCE_MAX_RB_NODES SACN_SOURCE_DETECTOR_MAX_SOURCES

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_UNIVERSE_DISCOVERY_SOURCE() malloc(sizeof(SacnUniverseDiscoverySource))
#define FREE_UNIVERSE_DISCOVERY_SOURCE(ptr) free(ptr)

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_UNIVERSE_DISCOVERY_SOURCE() etcpal_mempool_alloc(sacn_pool_srcdetect_sources)
#define FREE_UNIVERSE_DISCOVERY_SOURCE(ptr) etcpal_mempool_free(sacn_pool_srcdetect_sources, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_srcdetect_sources, SacnUniverseDiscoverySource, SACN_SOURCE_DETECTOR_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_srcdetect_rb_nodes, EtcPalRbNode, SACN_UNIVERSE_DISCOVERY_SOURCE_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

static EtcPalRbTree universe_discovery_sources;

/*********************** Private function prototypes *************************/

// Universe discovery source tree node management
static void universe_discovery_sources_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);

// Universe discovery source tree node management
static EtcPalRbNode* universe_discovery_source_node_alloc(void);
static void universe_discovery_source_node_dealloc(EtcPalRbNode* node);

/*************************** Function definitions ****************************/

etcpal_error_t init_universe_discovery_sources(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_srcdetect_sources);
  res |= etcpal_mempool_init(sacn_pool_srcdetect_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&universe_discovery_sources, remote_source_compare, universe_discovery_source_node_alloc,
                       universe_discovery_source_node_dealloc);
  }

  return res;
}

void deinit_universe_discovery_sources(void)
{
  etcpal_rbtree_clear_with_cb(&universe_discovery_sources, universe_discovery_sources_tree_dealloc);
}

etcpal_error_t add_sacn_universe_discovery_source(const EtcPalUuid* cid, const char* name,
                                                  SacnUniverseDiscoverySource** source_state)
{
  if (!SACN_ASSERT_VERIFY(cid) || !SACN_ASSERT_VERIFY(name))
    return kEtcPalErrSys;

  etcpal_error_t result = kEtcPalErrOk;
  SacnUniverseDiscoverySource* src = NULL;

  sacn_remote_source_t existing_handle = get_remote_source_handle(cid);
  if ((existing_handle != SACN_REMOTE_SOURCE_INVALID) &&
      etcpal_rbtree_find(&universe_discovery_sources, &existing_handle))
  {
    result = kEtcPalErrExists;
  }

  if (result == kEtcPalErrOk)
  {
    src = ALLOC_UNIVERSE_DISCOVERY_SOURCE();

    if (!src)
      result = kEtcPalErrNoMem;
  }

  sacn_remote_source_t handle = SACN_REMOTE_SOURCE_INVALID;
  if (result == kEtcPalErrOk)
    result = add_remote_source_handle(cid, &handle);

  if (result == kEtcPalErrOk)
  {
    src->handle = handle;
    strncpy(src->name, name, SACN_SOURCE_NAME_MAX_LEN);

    src->universes_dirty = true;
    src->num_universes = 0;
    src->last_notified_universe_count = 0;
    src->suppress_universe_limit_exceeded_notification = false;
#if SACN_DYNAMIC_MEM
    src->universes = calloc(INITIAL_CAPACITY, sizeof(uint16_t));
    src->universes_capacity = src->universes ? INITIAL_CAPACITY : 0;

    if (!src->universes)
      result = kEtcPalErrNoMem;
#else
    memset(src->universes, 0, sizeof(src->universes));
#endif
  }

  if (result == kEtcPalErrOk)
  {
    etcpal_timer_start(&src->expiration_timer, SACN_UNIVERSE_DISCOVERY_INTERVAL * 2);
    src->next_universe_index = 0;
    src->next_page = 0;

    result = etcpal_rbtree_insert(&universe_discovery_sources, src);
  }

  if (result == kEtcPalErrOk)
  {
    if (source_state)
      *source_state = src;
  }
  else
  {
    if (handle != SACN_REMOTE_SOURCE_INVALID)
      remove_remote_source_handle(handle);

    if (src)
    {
      CLEAR_BUF(src, universes);
      FREE_UNIVERSE_DISCOVERY_SOURCE(src);
    }
  }

  return result;
}

/*
 * If num_replacement_universes is too big, no replacement will occur, and this will return the maximum number for
 * num_replacement_universes that will fit. Otherwise, this will return num_replacement_universes.
 */
size_t replace_universe_discovery_universes(SacnUniverseDiscoverySource* source, size_t replace_start_index,
                                            const uint16_t* replacement_universes, size_t num_replacement_universes,
                                            size_t dynamic_universe_limit)
{
  if (!SACN_ASSERT_VERIFY(source))
    return 0;

#if SACN_DYNAMIC_MEM
  if ((dynamic_universe_limit != SACN_SOURCE_DETECTOR_INFINITE) &&
      ((replace_start_index + num_replacement_universes) > dynamic_universe_limit))
  {
    return (dynamic_universe_limit - replace_start_index);
  }
#else
  ETCPAL_UNUSED_ARG(dynamic_universe_limit);
#endif

#if SACN_DYNAMIC_MEM
  size_t capacity_fail_return_value = 0;
#else
  size_t capacity_fail_return_value = (SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE - replace_start_index);
#endif
  CHECK_CAPACITY(source, (replace_start_index + num_replacement_universes), universes, uint16_t,
                 SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE, capacity_fail_return_value);

  memcpy(&source->universes[replace_start_index], replacement_universes, num_replacement_universes * sizeof(uint16_t));
  source->num_universes = (replace_start_index + num_replacement_universes);

  return num_replacement_universes;
}

etcpal_error_t lookup_universe_discovery_source(sacn_remote_source_t handle, SacnUniverseDiscoverySource** source_state)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_REMOTE_SOURCE_INVALID) || !SACN_ASSERT_VERIFY(source_state))
    return kEtcPalErrSys;

  *source_state = (SacnUniverseDiscoverySource*)etcpal_rbtree_find(&universe_discovery_sources, &handle);
  return (*source_state) ? kEtcPalErrOk : kEtcPalErrNotFound;
}

SacnUniverseDiscoverySource* get_first_universe_discovery_source(EtcPalRbIter* iterator)
{
  if (!SACN_ASSERT_VERIFY(iterator))
    return NULL;

  etcpal_rbiter_init(iterator);
  return (SacnUniverseDiscoverySource*)etcpal_rbiter_first(iterator, &universe_discovery_sources);
}

SacnUniverseDiscoverySource* get_next_universe_discovery_source(EtcPalRbIter* iterator)
{
  if (!SACN_ASSERT_VERIFY(iterator))
    return NULL;

  return (SacnUniverseDiscoverySource*)etcpal_rbiter_next(iterator);
}

size_t get_num_universe_discovery_sources()
{
  return etcpal_rbtree_size(&universe_discovery_sources);
}

etcpal_error_t remove_sacn_universe_discovery_source(sacn_remote_source_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_REMOTE_SOURCE_INVALID))
    return kEtcPalErrSys;

  return etcpal_rbtree_remove_with_cb(&universe_discovery_sources, &handle, universe_discovery_sources_tree_dealloc);
}

void universe_discovery_sources_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  SacnUniverseDiscoverySource* source = (SacnUniverseDiscoverySource*)node->value;
  remove_remote_source_handle(source->handle);
  CLEAR_BUF(source, universes);
  FREE_UNIVERSE_DISCOVERY_SOURCE(source);
  universe_discovery_source_node_dealloc(node);
}

EtcPalRbNode* universe_discovery_source_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_srcdetect_rb_nodes);
#endif
}

void universe_discovery_source_node_dealloc(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_srcdetect_rb_nodes, node);
#endif
}

#endif  // SACN_SOURCE_DETECTOR_ENABLED
