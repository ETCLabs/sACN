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

/*********************** Private function prototypes *************************/

static int term_set_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* node_alloc(void);
static void node_dealloc(EtcPalRbNode* node);
static void source_remove_callback(const EtcPalRbTree* tree, EtcPalRbNode* node);

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
 * termination sets.
 *
 * [in] online_sources Array of online sources.
 * [in] num_online_sources Size of online_sources array.
 * [in,out] term_set_list List of termination sets in which to process the online sources.
 */
void mark_sources_online(const SacnRemoteSourceInternal* online_sources, size_t num_online_sources,
                         TerminationSet* term_set_list)
{
  for (const SacnRemoteSourceInternal* online_src = online_sources; online_src < online_sources + num_online_sources;
       ++online_src)
  {
    for (TerminationSet* ts = term_set_list; ts; ts = ts->next)
    {
      // Remove the source from the termination set if it exists, as it is confirmed online.
      etcpal_rbtree_remove_with_cb(&ts->sources, &online_src->handle, source_remove_callback);
    }
  }
}

/*
 * Process a list of sources that have timed out or terminated, creating new termination sets or
 * modifying them as necessary.
 *
 * [in] offline_sources Array of sources that have timed out or terminated.
 * [in] num_offline_sources Size of offline_sources array.
 * [in] sources_sending_dmx Array of all sources for which null start code data is currently
 *                          tracked on this universe.
 * [in] num_sending_dmx Size of sources_sending_dmx array.
 * [in,out] term_set_list List of termination sets in which to process the offline sources.
 * [in] expired_wait The current configured expired notification wait time for this universe.
 */
void mark_sources_offline(const SacnLostSourceInternal* offline_sources, size_t num_offline_sources,
                          const SacnRemoteSourceInternal* unknown_sources, size_t num_unknown_sources,
                          TerminationSet** term_set_list, uint32_t expired_wait)
{
  for (const SacnLostSourceInternal* offline_src = offline_sources; offline_src < offline_sources + num_offline_sources;
       ++offline_src)
  {
    bool found_source = false;

    TerminationSet** ts_ptr = term_set_list;
    for (; *ts_ptr; ts_ptr = &(*ts_ptr)->next)
    {
      // See if the source is in this termination set.
      TerminationSet* ts = *ts_ptr;
      TerminationSetSource* ts_src = etcpal_rbtree_find(&ts->sources, &offline_src->handle);
      if (ts_src)
      {
        if (!found_source)
        {
          found_source = true;
          if (!ts_src->offline)
          {
            // Mark the source as offline if it wasn't before
            ts_src->offline = true;
            ts_src->terminated = offline_src->terminated;
          }
        }
        else
        {
          // Remove the source from the later termination set, as it should only be marked offline
          // in one.
          etcpal_rbtree_remove(&ts->sources, ts_src);
          FREE_TERM_SET_SOURCE(ts_src);
        }
      }
    }

    // If we didn't find the source in any termination sets, we must create a new one.
    if (!found_source)
    {
      TerminationSet* ts_new = ALLOC_TERM_SET();
      if (ts_new)
      {
        etcpal_timer_start(&ts_new->wait_period, expired_wait);
        etcpal_rbtree_init(&ts_new->sources, term_set_source_compare, node_alloc, node_dealloc);
        ts_new->next = NULL;

        TerminationSetSource* ts_src_new = ALLOC_TERM_SET_SOURCE();
        if (ts_src_new)
        {
          ts_src_new->handle = offline_src->handle;
          ts_src_new->name = offline_src->name;
          ts_src_new->offline = true;
          ts_src_new->terminated = offline_src->terminated;
          if (etcpal_rbtree_insert(&ts_new->sources, ts_src_new) == kEtcPalErrOk)
          {
            // Add all of the other sources tracked by our universe that have sent at least one DMX
            // packet.
            for (const SacnRemoteSourceInternal* unknown_src = unknown_sources;
                 unknown_src < unknown_sources + num_unknown_sources; ++unknown_src)
            {
              ts_src_new = ALLOC_TERM_SET_SOURCE();
              if (ts_src_new)
              {
                ts_src_new->handle = unknown_src->handle;
                ts_src_new->name = unknown_src->name;
                ts_src_new->offline = false;
                ts_src_new->terminated = false;
                if (etcpal_rbtree_insert(&ts_new->sources, ts_src_new) != kEtcPalErrOk)
                  FREE_TERM_SET_SOURCE(ts_src_new);
              }
            }

            // At this point ts_ptr points to the end of the termination set list, so append the
            // new one
            *ts_ptr = ts_new;
          }
          else
          {
            FREE_TERM_SET_SOURCE(ts_src_new);
            FREE_TERM_SET(ts_new);
          }
        }
        else
        {
          FREE_TERM_SET(ts_new);
        }
      }
    }
  }
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
  TerminationSet* ts = *term_set_list;
  TerminationSet* last_ts = *term_set_list;
  while (ts)
  {
    bool remove_ts = false;

    if (etcpal_timer_is_expired(&ts->wait_period))
    {
      if (etcpal_rbtree_size(&ts->sources) == 0)
      {
        // No sources are present in the termination set and the wait period is expired; remove it.
        remove_ts = true;
      }
      else
      {
        // Check each source to in the termination set to determine whether it is online or offline.
        // If the source is already gone from our tracked sources (i.e. it was removed as part of
        // another termination set), it is considered to be offline.
        size_t num_expired_sources_this_ts = 0;
        remove_ts = true;

        EtcPalRbIter ts_src_it;
        etcpal_rbiter_init(&ts_src_it);

        TerminationSetSource* ts_src = (TerminationSetSource*)etcpal_rbiter_first(&ts_src_it, &ts->sources);
        while (ts_src)
        {
          if (ts_src->offline)
          {
            if (add_lost_source(sources_lost, ts_src->handle, get_remote_source_cid(ts_src->handle), ts_src->name,
                                ts_src->terminated))
            {
              ++num_expired_sources_this_ts;
            }
            else if (SACN_CAN_LOG(ETCPAL_LOG_ERR))
            {
              char cid_str[ETCPAL_UUID_BYTES];
              etcpal_uuid_to_string(get_remote_source_cid(ts_src->handle), cid_str);
              SACN_LOG_ERR("Couldn't allocate memory to notify that source %s was lost!", cid_str);
            }
          }
          else
          {
            // The first source we find to be still online cancels the processing of this
            // termination set. Roll back the expired_sources array by the number of sources we've
            // added from this set, and do not remove this one yet.
            sources_lost->num_lost_sources -= num_expired_sources_this_ts;
            remove_ts = false;
            break;
          }
          ts_src = etcpal_rbiter_next(&ts_src_it);
        }
      }
    }

    if (remove_ts)
    {
      TerminationSet* to_remove = ts;
      if (last_ts == ts)
      {
        // Replace the head of the list
        *term_set_list = ts->next;
        last_ts = ts->next;
        ts = ts->next;
      }
      else
      {
        // Remove from the list
        last_ts->next = ts->next;
        ts = ts->next;
      }
      etcpal_rbtree_clear_with_cb(&to_remove->sources, source_remove_callback);
      FREE_TERM_SET(to_remove);
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
    etcpal_rbtree_clear_with_cb(&to_remove->sources, source_remove_callback);
    FREE_TERM_SET(to_remove);
  }
}

int term_set_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  sacn_remote_source_t* a = (sacn_remote_source_t*)value_a;
  sacn_remote_source_t* b = (sacn_remote_source_t*)value_b;
  return (*a > *b) - (*a < *b);
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
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacn_pool_source_loss_rb_nodes, node);
#endif
}

static void source_remove_callback(const EtcPalRbTree* tree, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(tree);
  FREE_TERM_SET_SOURCE(node->value);
  node_dealloc(node);
}

#endif  // SACN_RECEIVER_ENABLED
