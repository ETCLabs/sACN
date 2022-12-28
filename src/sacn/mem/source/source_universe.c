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

/**************************** Private constants ******************************/

#define SACN_SOURCE_UNIVERSE_MAX_UNIVERSES (SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE * SACN_SOURCE_MAX_SOURCES)
#define SACN_SOURCE_UNIVERSE_MAX_RB_NODES (SACN_SOURCE_UNIVERSE_MAX_UNIVERSES * 2)  // Two universe trees

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM

/* Macros for dynamic allocation. */
#define ALLOC_UNIVERSE() malloc(sizeof(SacnSourceUniverse))
#define FREE_UNIVERSE(ptr)               \
  if (SACN_ASSERT_VERIFY(ptr))           \
  {                                      \
    CLEAR_BUF((ptr), unicast_dests);     \
    CLEAR_BUF(&(ptr)->netints, netints); \
    free(ptr);                           \
  }

#else  // SACN_DYNAMIC_MEM

/* Macros for static allocation, which is done using etcpal_mempool. */
#define ALLOC_UNIVERSE() etcpal_mempool_alloc(sacn_pool_source_universes)
#define FREE_UNIVERSE(ptr) etcpal_mempool_free(sacn_pool_source_universes, ptr)

#endif  // SACN_DYNAMIC_MEM

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_source_universes, SacnSourceUniverse, SACN_SOURCE_UNIVERSE_MAX_UNIVERSES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_source_universe_rb_nodes, EtcPalRbNode, SACN_SOURCE_UNIVERSE_MAX_RB_NODES);
#endif  // !SACN_DYNAMIC_MEM

/********************** Private function declarations ************************/

static void source_universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static int source_universe_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* source_universe_node_alloc(void);
static void source_universe_node_dealloc(EtcPalRbNode* node);

/*************************** Function definitions ****************************/

// Needs lock
etcpal_error_t add_sacn_source_universe(SacnSource* source, const SacnSourceUniverseConfig* config,
                                        const SacnNetintConfig* netint_config, SacnSourceUniverse** universe_state)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(config) || !SACN_ASSERT_VERIFY(universe_state))
    return kEtcPalErrSys;

  etcpal_error_t result = kEtcPalErrOk;

#if SACN_DYNAMIC_MEM
  // Make sure to check against universe_count_max.
  if ((source->universe_count_max != SACN_SOURCE_INFINITE_UNIVERSES) &&
      (get_num_source_universes(source) >= source->universe_count_max))
  {
    result = kEtcPalErrNoMem;  // No room to allocate additional universe.
  }
#else
  if (get_num_source_universes(source) >= SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE)
    result = kEtcPalErrNoMem;  // This would exceed the number of universes allowed for this source.
#endif

  if (result == kEtcPalErrOk)
  {
    if (etcpal_rbtree_find(&source->universes, &config->universe))
      result = kEtcPalErrExists;
  }

  SacnSourceUniverse* universe = NULL;
  if (result == kEtcPalErrOk)
  {
    universe = ALLOC_UNIVERSE();
    if (!universe)
      result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
  {
    memset(universe, 0, sizeof(SacnSourceUniverse));

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
#else
    memset(universe->unicast_dests, 0, sizeof(universe->unicast_dests));
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
    result = etcpal_rbtree_insert(&source->universes, universe);

  if (result == kEtcPalErrOk)
  {
    *universe_state = universe;
  }
  else if (universe)
  {
    FREE_UNIVERSE(universe);
  }

  return result;
}

// Needs lock
etcpal_error_t lookup_source_and_universe(sacn_source_t source, uint16_t universe, SacnSource** source_state,
                                          SacnSourceUniverse** universe_state)
{
  if (!SACN_ASSERT_VERIFY(source != SACN_SOURCE_INVALID) || !SACN_ASSERT_VERIFY(source_state) ||
      !SACN_ASSERT_VERIFY(universe_state))
  {
    return kEtcPalErrSys;
  }

  etcpal_error_t result = lookup_source(source, source_state);

  if (result == kEtcPalErrOk)
    result = lookup_universe(*source_state, universe, universe_state);

  return result;
}

// Needs lock
size_t get_num_source_universes(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return 0;

  return etcpal_rbtree_size(&source->universes);
}

// Needs lock
etcpal_error_t lookup_universe(SacnSource* source, uint16_t universe, SacnSourceUniverse** universe_state)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe_state))
    return kEtcPalErrSys;

  *universe_state = etcpal_rbtree_find(&source->universes, &universe);
  return *universe_state ? kEtcPalErrOk : kEtcPalErrNotFound;
}

// Needs lock
etcpal_error_t mark_source_universe_for_removal(SacnSource* source, SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe) ||
      !SACN_ASSERT_VERIFY(etcpal_rbtree_find(&source->universes, universe) != NULL) ||
      !SACN_ASSERT_VERIFY(etcpal_rbtree_find(&source->universes_to_remove, universe) == NULL))
  {
    return kEtcPalErrSys;
  }

  etcpal_error_t res = etcpal_rbtree_insert(&source->universes_to_remove, universe);
  SACN_ASSERT_VERIFY(res != kEtcPalErrExists);
  SACN_ASSERT_VERIFY(res != kEtcPalErrInvalid);

  return res;
}

// Needs lock - DO NOT call while iterating source universes!
void remove_universes_marked_for_removal(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return;

  bool any_removed = false;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  for (SacnSourceUniverse* to_remove = etcpal_rbiter_first(&iter, &source->universes_to_remove); to_remove;
       to_remove = etcpal_rbiter_next(&iter))
  {
    // Just remove the node, don't free the universe state itself yet
    SACN_ASSERT_VERIFY(etcpal_rbtree_remove(&source->universes, to_remove) == kEtcPalErrOk);
    any_removed = true;
  }

  // The main universe tree is up-to-date - now just clear the "to remove" tree (this time using
  // source_universe_tree_dealloc to free the universe state)
  if (any_removed)
  {
    SACN_ASSERT_VERIFY(etcpal_rbtree_clear_with_cb(&source->universes_to_remove, source_universe_tree_dealloc) ==
                       kEtcPalErrOk);
  }
}

// Needs lock
SacnSourceUniverse* get_first_source_universe(SacnSource* source, EtcPalRbIter* iterator)
{
  if (!SACN_ASSERT_VERIFY(iterator))
    return NULL;

  etcpal_rbiter_init(iterator);
  return (SacnSourceUniverse*)etcpal_rbiter_first(iterator, &source->universes);
}

// Needs lock
SacnSourceUniverse* get_next_source_universe(EtcPalRbIter* iterator)
{
  if (!SACN_ASSERT_VERIFY(iterator))
    return NULL;

  return (SacnSourceUniverse*)etcpal_rbiter_next(iterator);
}

// Needs lock
void init_source_universe_state(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return;

  etcpal_rbtree_init(&source->universes, source_universe_compare, source_universe_node_alloc,
                     source_universe_node_dealloc);
  etcpal_rbtree_init(&source->universes_to_remove, source_universe_compare, source_universe_node_alloc,
                     source_universe_node_dealloc);
}

void clear_source_universes(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return;

  etcpal_rbtree_clear(&source->universes_to_remove);
  etcpal_rbtree_clear_with_cb(&source->universes, source_universe_tree_dealloc);
}

etcpal_error_t init_source_universes(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_source_universes);
  res |= etcpal_mempool_init(sacn_pool_source_universe_rb_nodes);
#endif

  return res;
}

// Needs lock
void source_universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);
  FREE_UNIVERSE((SacnSourceUniverse*)node->value);
  source_universe_node_dealloc(node);
}

int source_universe_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  const uint16_t* a = (const uint16_t*)value_a;
  const uint16_t* b = (const uint16_t*)value_b;
  return (*a > *b) - (*a < *b);
}

EtcPalRbNode* source_universe_node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_source_universe_rb_nodes);
#endif
}

void source_universe_node_dealloc(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_source_universe_rb_nodes, node);
#endif
}

#endif  // SACN_SOURCE_ENABLED
