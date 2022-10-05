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

#include "sacn/private/source_loss.h"

#include "etcpal/common.h"
#include "etcpal/mempool.h"
#include "sacn/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"

#if SACN_RECEIVER_ENABLED

/****************************** Private macros *******************************/

#if SACN_DYNAMIC_MEM
#define ALLOC_TERM_SET_SOURCE() malloc(sizeof(TerminationSetSource))
#define ALLOC_TERM_SET() malloc(sizeof(TerminationSet))
#define FREE_TERM_SET_SOURCE(ptr) free(ptr)
#define FREE_TERM_SET(ptr) free(ptr)
#else
#define ALLOC_TERM_SET_SOURCE() etcpal_mempool_alloc(sacn_pool_term_set_sources)
#define ALLOC_TERM_SET() etcpal_mempool_alloc(sacn_pool_term_sets)
#define FREE_TERM_SET_SOURCE(ptr) etcpal_mempool_free(sacn_pool_term_set_sources, ptr)
#define FREE_TERM_SET(ptr) etcpal_mempool_free(sacn_pool_term_sets, ptr)
#endif

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_term_set_sources, TerminationSetSource, SACN_MAX_TERM_SET_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_term_sets, TerminationSet, SACN_MAX_TERM_SETS);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_source_loss_rb_nodes, EtcPalRbNode, SACN_SOURCE_LOSS_MAX_RB_NODES);
#endif

static EtcPalRbTree term_set_sources;

/*********************** Private function prototypes *************************/

static int term_set_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* node_alloc(void);
static void node_dealloc(EtcPalRbNode* node);
static void source_remove_callback(const EtcPalRbTree* tree, EtcPalRbNode* node);
static void source_remove_from_ts_callback(const EtcPalRbTree* tree, EtcPalRbNode* node);
static etcpal_error_t insert_new_ts_src(TerminationSetSource* ts_src_new, TerminationSet* ts_new);
static TerminationSetSource* find_existing_ts_src(uint16_t universe, sacn_remote_source_t handle);
static void remove_term_set_from_list(TerminationSet** term_set_list, TerminationSet* to_remove,
                                      TerminationSet* last_ts);

/*************************** Function definitions ****************************/

/*
 * Initialize the source loss module. Returns the result of the initialization.
 */
etcpal_error_t sacn_source_loss_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacn_pool_term_set_sources);
  res |= etcpal_mempool_init(sacn_pool_term_sets);
  res |= etcpal_mempool_init(sacn_pool_source_loss_rb_nodes);
#endif

  etcpal_rbtree_init(&term_set_sources, term_set_source_compare, node_alloc, node_dealloc);

  return res;
}

/*
 * Deinitialize the source loss module.
 */
void sacn_source_loss_deinit(void)
{
  /* Nothing to do here. */
}

/*
 * Remove a list of sources that have been determined to still be online from all applicable
 * termination sets. Termination sets that become empty are immediately removed.
 *
 * [in] universe The universe the sources are part of.
 * [in] online_sources Array of online sources.
 * [in] num_online_sources Size of online_sources array.
 * [in,out] term_set_list List of termination sets in which to process the online sources.
 */
void mark_sources_online(uint16_t universe, const SacnRemoteSourceInternal* online_sources, size_t num_online_sources,
                         TerminationSet** term_set_list)
{
  if (!SACN_ASSERT_VERIFY(online_sources) || !SACN_ASSERT_VERIFY(term_set_list))
    return;

  for (const SacnRemoteSourceInternal* online_src = online_sources; online_src < online_sources + num_online_sources;
       ++online_src)
  {
    TerminationSet* ts = *term_set_list;
    TerminationSet* last_ts = NULL;
    while (ts)
    {
      // Remove the source from the termination set if it exists, as it is confirmed online.
      TerminationSetSourceKey ts_src_key;
      ts_src_key.handle = online_src->handle;
      ts_src_key.universe = universe;
      etcpal_rbtree_remove_with_cb(&ts->sources, &ts_src_key, source_remove_from_ts_callback);

      if (etcpal_rbtree_size(&ts->sources) == 0)  // Remove empty termination sets immediately.
      {
        TerminationSet* to_remove = ts;
        ts = ts->next;

        remove_term_set_from_list(term_set_list, to_remove, last_ts);
      }
      else
      {
        last_ts = ts;
        ts = ts->next;
      }
    }
  }
}

/*
 * Process a list of sources that have timed out or terminated, creating new termination sets or
 * modifying them as necessary.
 *
 * [in] universe The universe the sources are part of.
 * [in] offline_sources Array of sources that have timed out or terminated.
 * [in] num_offline_sources Size of offline_sources array.
 * [in] unknown_sources Array of all sources for which null start code data hasn't been received in the last tick. These
 * haven't timed out or terminated yet. Can be null if there aren't any sources in this category.
 * [in] num_unknown_sources Size of unknown_sources array. If it's null, this must be 0.
 * [in,out] term_set_list List of termination sets in which to process the offline sources.
 * [in] expired_wait The current configured expired notification wait time for this universe.
 */
etcpal_error_t mark_sources_offline(uint16_t universe, const SacnLostSourceInternal* offline_sources,
                                    size_t num_offline_sources, const SacnRemoteSourceInternal* unknown_sources,
                                    size_t num_unknown_sources, TerminationSet** term_set_list, uint32_t expired_wait)
{
  if (!SACN_ASSERT_VERIFY(term_set_list))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

  for (const SacnLostSourceInternal* offline_src = offline_sources;
       offline_src && (offline_src < (offline_sources + num_offline_sources)); ++offline_src)
  {
    TerminationSetSource* ts_src = find_existing_ts_src(universe, offline_src->handle);

    if (ts_src)
    {
      if (!ts_src->offline)
      {
        // Mark the source as offline if it wasn't before
        ts_src->offline = true;
        ts_src->terminated = offline_src->terminated;
      }
    }
    else  // If we didn't find the source in any termination sets, we must create a new one.
    {
      TerminationSet* ts_new = ALLOC_TERM_SET();
      if (!ts_new)
        res = kEtcPalErrNoMem;

      TerminationSetSource* ts_src_new = NULL;
      if (res == kEtcPalErrOk)
      {
        etcpal_timer_start(&ts_new->wait_period, expired_wait);
        etcpal_rbtree_init(&ts_new->sources, term_set_source_compare, node_alloc, node_dealloc);
        ts_new->next = NULL;

        ts_src_new = ALLOC_TERM_SET_SOURCE();
        if (!ts_src_new)
        {
          FREE_TERM_SET(ts_new);
          res = kEtcPalErrNoMem;
        }
      }

      if (res == kEtcPalErrOk)
      {
        ts_src_new->key.handle = offline_src->handle;
        ts_src_new->key.universe = universe;
        ts_src_new->name = offline_src->name;
        ts_src_new->offline = true;
        ts_src_new->terminated = offline_src->terminated;

        res = insert_new_ts_src(ts_src_new, ts_new);
        if (res != kEtcPalErrOk)
        {
          FREE_TERM_SET_SOURCE(ts_src_new);
          FREE_TERM_SET(ts_new);
        }
      }

      if (res == kEtcPalErrOk)
      {
        // Add all of the other sources tracked by our universe that have sent at least one DMX
        // packet (exclude those that are already part of a termination set).
        for (const SacnRemoteSourceInternal* unknown_src = unknown_sources;
             unknown_src && (unknown_src < (unknown_sources + num_unknown_sources)); ++unknown_src)
        {
          if (!find_existing_ts_src(universe, unknown_src->handle))
          {
            ts_src_new = ALLOC_TERM_SET_SOURCE();
            if (ts_src_new)
            {
              ts_src_new->key.handle = unknown_src->handle;
              ts_src_new->key.universe = universe;
              ts_src_new->name = unknown_src->name;
              ts_src_new->offline = false;
              ts_src_new->terminated = false;

              res = insert_new_ts_src(ts_src_new, ts_new);
              if (res != kEtcPalErrOk)
              {
                FREE_TERM_SET_SOURCE(ts_src_new);
                break;
              }
            }
            else
            {
              res = kEtcPalErrNoMem;
              break;
            }
          }
        }

        // Append the new termination set to the end of the list.
        TerminationSet** ts_ptr = term_set_list;
        while (*ts_ptr)
          ts_ptr = &(*ts_ptr)->next;

        *ts_ptr = ts_new;
      }
    }
  }

  return res;
}

/*
 * Process the current termination sets and determine if any sources are expired and should be
 * removed.
 *
 * [in,out] term_set_list The list of termination sets for a universe.
 * [out] sources_lost Notification struct to fill in with the lost sources. The lost_sources and
 *                    num_lost_sources members will be modified.
 */
void get_expired_sources(TerminationSet** term_set_list, SourcesLostNotification* sources_lost)
{
  if (!SACN_ASSERT_VERIFY(term_set_list) || !SACN_ASSERT_VERIFY(sources_lost))
    return;

  TerminationSet* ts = *term_set_list;
  TerminationSet* last_ts = NULL;
  while (ts)
  {
    bool remove_ts = false;

    if (etcpal_timer_is_expired(&ts->wait_period))
    {
      // Check each source in the termination set to determine whether it is online or offline. Each termination set has
      // at least one unique source.
      size_t num_expired_sources_this_ts = 0;
      remove_ts = true;

      EtcPalRbIter ts_src_it;
      etcpal_rbiter_init(&ts_src_it);

      TerminationSetSource* ts_src = (TerminationSetSource*)etcpal_rbiter_first(&ts_src_it, &ts->sources);
      while (ts_src)
      {
        if (ts_src->offline)
        {
          const EtcPalUuid* ts_src_cid = get_remote_source_cid(ts_src->key.handle);
          if (SACN_ASSERT_VERIFY(ts_src_cid))
          {
            if (add_lost_source(sources_lost, ts_src->key.handle, ts_src_cid, ts_src->name, ts_src->terminated))
            {
              ++num_expired_sources_this_ts;
            }
            else if (SACN_CAN_LOG(ETCPAL_LOG_ERR))
            {
              char cid_str[ETCPAL_UUID_BYTES];
              etcpal_uuid_to_string(ts_src_cid, cid_str);
              SACN_LOG_ERR("Couldn't allocate memory to notify that source %s was lost!", cid_str);
            }
          }
        }
        else
        {
          // The first source we find to be still unknown cancels the processing of this
          // termination set. Roll back the expired_sources array by the number of sources we've
          // added from this set, and do not remove this one yet.
          sources_lost->num_lost_sources -= num_expired_sources_this_ts;
          remove_ts = false;
          break;
        }
        ts_src = etcpal_rbiter_next(&ts_src_it);
      }
    }

    if (remove_ts)
    {
      TerminationSet* to_remove = ts;
      ts = ts->next;

      remove_term_set_from_list(term_set_list, to_remove, last_ts);
    }
    else
    {
      last_ts = ts;
      ts = ts->next;
    }
  }
}

void clear_term_set_list(TerminationSet* list)
{
  TerminationSet* entry = list;
  while (entry)
  {
    TerminationSet* to_remove = entry;
    entry = entry->next;
    etcpal_rbtree_clear_with_cb(&to_remove->sources, source_remove_from_ts_callback);
    FREE_TERM_SET(to_remove);
  }
}

int term_set_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  TerminationSetSourceKey* a = (TerminationSetSourceKey*)value_a;
  TerminationSetSourceKey* b = (TerminationSetSourceKey*)value_b;

  if (a->handle > b->handle)
    return 1;

  if (a->handle == b->handle)
  {
    if (a->universe > b->universe)
      return 1;
    if (a->universe == b->universe)
      return 0;
  }

  return -1;
}

EtcPalRbNode* node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacn_pool_source_loss_rb_nodes);
#endif
}

void node_dealloc(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_source_loss_rb_nodes, node);
#endif
}

// Only to be used when removing from the main term_set_sources rbtree.
static void source_remove_callback(const EtcPalRbTree* tree, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_TERM_SET_SOURCE(node->value);
  node_dealloc(node);
}

// This version is only meant to be used when removing from a termination set because it erases the source from the main
// term_set_sources rbtree.
static void source_remove_from_ts_callback(const EtcPalRbTree* tree, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(tree);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  etcpal_rbtree_remove_with_cb(&term_set_sources, node->value, source_remove_callback);
  node_dealloc(node);
}

// Insert a new termination set source into the main term_set_sources rbtree as well as a termination set's rbtree.
etcpal_error_t insert_new_ts_src(TerminationSetSource* ts_src_new, TerminationSet* ts_new)
{
  if (!SACN_ASSERT_VERIFY(ts_src_new) || !SACN_ASSERT_VERIFY(ts_new))
    return kEtcPalErrSys;

  etcpal_error_t res = etcpal_rbtree_insert(&term_set_sources, ts_src_new);

  if (res == kEtcPalErrOk)
  {
    res = etcpal_rbtree_insert(&ts_new->sources, ts_src_new);

    if (res != kEtcPalErrOk)
      etcpal_rbtree_remove(&term_set_sources, ts_src_new);
  }

  return res;
}

TerminationSetSource* find_existing_ts_src(uint16_t universe, sacn_remote_source_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_REMOTE_SOURCE_INVALID))
    return NULL;

  TerminationSetSourceKey ts_src_key;
  ts_src_key.handle = handle;
  ts_src_key.universe = universe;
  return etcpal_rbtree_find(&term_set_sources, &ts_src_key);
}

void remove_term_set_from_list(TerminationSet** term_set_list, TerminationSet* to_remove, TerminationSet* last_ts)
{
  if (!SACN_ASSERT_VERIFY(term_set_list) || !SACN_ASSERT_VERIFY(to_remove))
    return;

  if (last_ts == NULL)  // Replace the head of the list
    *term_set_list = to_remove->next;
  else  // Remove from the list
    last_ts->next = to_remove->next;

  etcpal_rbtree_clear_with_cb(&to_remove->sources, source_remove_from_ts_callback);
  FREE_TERM_SET(to_remove);
}

#endif  // SACN_RECEIVER_ENABLED
