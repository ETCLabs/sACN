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

#include "sacn/dmx_merger.h"
#include "sacn/private/common.h"
#include "sacn/private/dmx_merger.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_DMX_MERGER_ENABLED || DOXYGEN

/****************************** Private macros *******************************/

#define PAP_ACTIVE(source_state) (!(source_state)->source.using_universe_priority)
#define SET_PAP_ACTIVE(source_state) ((source_state)->source.using_universe_priority = false)
#define SET_PAP_INACTIVE(source_state) ((source_state)->source.using_universe_priority = true)

// Used for a few cases where we need a PAP of 0 if beyond the level count.
#define CALC_SRC_PAP(source_state, slot) \
  ((slot < source_state->source.valid_level_count) ? source_state->source.address_priority[slot] : 0)

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

#if SACN_DYNAMIC_MEM
#define ALLOC_SOURCE_STATE() malloc(sizeof(SourceState))
#define FREE_SOURCE_STATE(ptr) free(ptr)
#define ALLOC_MERGER_STATE() malloc(sizeof(MergerState))
#define FREE_MERGER_STATE(ptr) free(ptr)
#define ALLOC_DMX_MERGER_RB_NODE() malloc(sizeof(EtcPalRbNode))
#define FREE_DMX_MERGER_RB_NODE(ptr) free(ptr)
#else
#define ALLOC_SOURCE_STATE() etcpal_mempool_alloc(sacn_pool_merge_source_states)
#define FREE_SOURCE_STATE(ptr) etcpal_mempool_free(sacn_pool_merge_source_states, ptr)
#define ALLOC_MERGER_STATE() etcpal_mempool_alloc(sacn_pool_merge_merger_states)
#define FREE_MERGER_STATE(ptr) etcpal_mempool_free(sacn_pool_merge_merger_states, ptr)
#define ALLOC_DMX_MERGER_RB_NODE() etcpal_mempool_alloc(sacn_pool_merge_rb_nodes)
#define FREE_DMX_MERGER_RB_NODE(ptr) etcpal_mempool_free(sacn_pool_merge_rb_nodes, ptr)
#endif

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_merge_source_states, SourceState,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS));
ETCPAL_MEMPOOL_DEFINE(sacn_pool_merge_merger_states, MergerState, SACN_DMX_MERGER_MAX_MERGERS);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_merge_rb_nodes, EtcPalRbNode,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS) +
                          SACN_DMX_MERGER_MAX_MERGERS);
#endif

static IntHandleManager merger_handle_mgr;
static EtcPalRbTree mergers;

/**************************** Private function declarations ******************************/

static int merger_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);

static EtcPalRbNode* dmx_merger_rb_node_alloc_func(void);
static void dmx_merger_rb_node_dealloc_func(EtcPalRbNode* node);

static bool merger_handle_in_use(int handle_val, void* cookie);
static bool source_handle_in_use(int handle_val, void* cookie);

static etcpal_error_t add_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t id_to_use,
                                 sacn_dmx_merger_source_t* id_result);

static void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_levels,
                          uint16_t new_levels_count);
static void update_pap(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                       uint16_t address_priorities_count);
static void update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority);
static void update_levels_single_source(MergerState* merger, SourceState* source, const uint8_t* new_levels,
                                        size_t old_levels_count, size_t new_levels_count);
static void update_levels_multi_source(MergerState* merger, SourceState* source, const uint8_t* new_levels,
                                       size_t old_levels_count, size_t new_levels_count);
static void update_pap_single_source(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                     size_t old_pap_count, size_t new_pap_count);
static void update_pap_multi_source(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                    size_t old_pap_count, size_t new_pap_count);
static void update_universe_priority_single_source(MergerState* merger, SourceState* source, uint8_t pap);
static void update_universe_priority_multi_source(MergerState* merger, SourceState* source, uint8_t pap);
static void merge_new_level(MergerState* merger, const SourceState* source, size_t slot);
static void merge_new_priority(MergerState* merger, const SourceState* source, size_t slot);
static void recalculate_winning_level(MergerState* merger, const SourceState* source, size_t slot);
static void recalculate_winning_priority(MergerState* merger, const SourceState* source, size_t slot);
static void recalculate_pap_active(MergerState* merger);
static void recalculate_universe_priority(MergerState* merger);

static void free_source_state_lookup_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_mergers_node(const EtcPalRbTree* self, EtcPalRbNode* node);

static SourceState* construct_source_state(sacn_dmx_merger_source_t handle);
static MergerState* construct_merger_state(sacn_dmx_merger_t handle, const SacnDmxMergerConfig* config);

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN DMX Merger module. Internal function called from sacn_init(). */
etcpal_error_t sacn_dmx_merger_init(void)
{
#if (!SACN_DYNAMIC_MEM && ((SACN_DMX_MERGER_MAX_MERGERS <= 0) || (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER <= 0)))
  etcpal_error_t res = kEtcPalErrInvalid;
#else
  etcpal_error_t res = kEtcPalErrOk;
#endif

#if !SACN_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacn_pool_merge_source_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacn_pool_merge_merger_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacn_pool_merge_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&mergers, merger_state_lookup_compare_func, dmx_merger_rb_node_alloc_func,
                       dmx_merger_rb_node_dealloc_func);
    init_int_handle_manager(&merger_handle_mgr, -1, merger_handle_in_use, NULL);
  }

  return res;
}

/* Deinitialize the sACN DMX Merger module. Internal function called from sacn_deinit(). */
void sacn_dmx_merger_deinit(void)
{
  if (sacn_lock())
  {
    etcpal_rbtree_clear_with_cb(&mergers, free_mergers_node);
    sacn_unlock();
  }
}

/**
 * @brief Create a new merger instance.
 *
 * Creates a new merger that uses the passed in config data.  The application owns all buffers
 * in the config, so be sure to call dmx_merger_destroy before destroying the buffers.
 *
 * @param[in] config Configuration parameters for the DMX merger to be created.
 * @param[out] handle Filled in on success with a handle to the merger.
 * @return #kEtcPalErrOk: Merger created successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this merger, or maximum number of mergers has been reached.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_create(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Validate arguments.
  if (result == kEtcPalErrOk)
  {
    if (!config || !handle || !config->levels)
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
#if SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER
    if (!config->per_address_priorities)
      result = kEtcPalErrInvalid;
#endif

#if SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER
    if (!config->owners)
      result = kEtcPalErrInvalid;
#endif
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = create_sacn_dmx_merger(config, handle);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Destroy a merger instance.
 *
 * Tears down the merger and cleans up its resources.
 *
 * @param[in] handle Handle to the merger to destroy.
 * @return #kEtcPalErrOk: Merger destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_destroy(sacn_dmx_merger_t handle)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Validate handle.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_DMX_MERGER_INVALID)
      result = kEtcPalErrNotFound;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = destroy_sacn_dmx_merger(handle);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Adds a new source to the merger.
 *
 * Adds a new source to the merger, if the maximum number of sources hasn't been reached.
 * The filled in source id is used for two purposes:
 *   - It is the handle for calls that need to access the source data.
 *   - It is the source identifer that is put into the owners buffer that was passed
 *     in the DmxMergerUniverseConfig structure when creating the merger.
 *
 * @param[in] merger The handle to the merger.
 * @param[out] source_id Filled in on success with the source id.
 * @return #kEtcPalErrOk: Source added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this source, or the max number of sources has been reached.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_add_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t* source_id)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  if (result == kEtcPalErrOk)
  {
    if (!source_id || (merger == SACN_DMX_MERGER_INVALID))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = add_sacn_dmx_merger_source(merger, source_id);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Removes a source from the merger.
 *
 * Removes the source from the merger.  This causes the merger to recalculate the outputs.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source to remove.
 * @return #kEtcPalErrOk: Source removed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_remove_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check if the handles are invalid.
  if (result == kEtcPalErrOk)
  {
    if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = remove_sacn_dmx_merger_source(merger, source);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Gets a read-only view of the source data.
 *
 * Looks up the source data and returns a pointer to the data or NULL if it doesn't exist.
 * This pointer is owned by the library, and must not be modified by the application.
 * The pointer will only be valid until the source or merger is removed.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source.
 * @return The pointer to the source data, or NULL if the source wasn't found.
 */
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)
{
  const SacnDmxMergerSource* result = NULL;

  if ((merger != SACN_DMX_MERGER_INVALID) && (source != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    if (sacn_lock())
    {
      MergerState* merger_state = NULL;
      SourceState* source_state = NULL;

      lookup_state(merger, source, &merger_state, &source_state);

      if (source_state)
        result = &source_state->source;

      sacn_unlock();
    }
  }

  return result;
}

/**
 * @brief Updates a source's levels and recalculates outputs.
 *
 * This function updates the levels of the specified source, and then triggers the recalculation of each slot. Only
 * slots within the valid level range will ever be factored into the merge. If the level count increased (it is 0
 * initially), previously inputted priorities will be factored into the recalculation for the added slots. However, if
 * the level count decreased, the slots that were lost will be released and will no longer be part of the merge. For
 * each slot, the source will only be included in the merge if it has a priority at that slot. Otherwise the level will
 * be saved for when a priority is eventually inputted.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source to modify.
 * @param[in] new_levels The new DMX levels to be copied in, starting from the first slot.
 * @param[in] new_levels_count The length of new_levels. Only slots within this range will ever be factored into the
 * merge.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_update_levels(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                             const uint8_t* new_levels, size_t new_levels_count)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Validate arguments.
  if (result == kEtcPalErrOk)
  {
    if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
      result = kEtcPalErrInvalid;
    if (new_levels_count > DMX_ADDRESS_COUNT)
      result = kEtcPalErrInvalid;
    if (!new_levels || (new_levels_count == 0))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = update_sacn_dmx_merger_levels(merger, source, new_levels, new_levels_count);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  // Return the final etcpal_error_t result.
  return result;
}

/**
 * @brief Updates a source's per-address priorities (PAP) and recalculates outputs.
 *
 * This function updates the per-address priorities (PAP) of the specified source, and then triggers the recalculation
 * of each slot within the current valid level count (thus no merging will occur if levels haven't been inputted yet).
 * Priorities beyond this count are saved, and eventually merged once levels beyond this count are inputted. For each
 * slot, the source will only be included in the merge if it has a priority at that slot.
 *
 * If PAP is not specified for all levels, then the remaining levels will default to a PAP of 0. To remove PAP for this
 * source and revert to the universe priority, call sacn_dmx_merger_remove_pap.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source to modify.
 * @param[in] pap The per-address priorities to be copied in, starting from the first slot.
 * @param[in] pap_count The length of pap.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_update_pap(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t* pap,
                                          size_t pap_count)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Validate arguments.
  if (result == kEtcPalErrOk)
  {
    if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
      result = kEtcPalErrInvalid;
    if (pap_count > DMX_ADDRESS_COUNT)
      result = kEtcPalErrInvalid;
    if (!pap || (pap_count == 0))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = update_sacn_dmx_merger_pap(merger, source, pap, pap_count);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  // Return the final etcpal_error_t result.
  return result;
}

/**
 * @brief Updates a source's universe priority and recalculates outputs.
 *
 * This function updates the universe priority of the specified source, and then triggers the recalculation of each
 * slot within the current valid level count (thus no merging will occur if levels haven't been inputted yet).
 * Priorities for slots beyond this count are saved, and eventually merged once levels beyond this count are inputted.
 * For each slot, the source will only be included in the merge if it has a priority at that slot.
 *
 * If this source currently has per-address priorities (PAP) via sacn_dmx_merger_update_pap, then the
 * universe priority can have no effect on the merge results until the application calls sacn_dmx_merger_remove_pap, at
 * which point the priorities of each slot will revert to the universe priority passed in here.
 *
 * If this source doesn't have PAP, then the universe priority is converted into PAP for each slot. These are the
 * priorities used for the merge. This means a universe priority of 0 will be converted to a PAP of 1.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source to modify.
 * @param[in] universe_priority The universe-level priority of the source.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_update_universe_priority(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                        uint8_t universe_priority)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Validate arguments.
  if ((result == kEtcPalErrOk) && ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID)))
    result = kEtcPalErrInvalid;

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = update_sacn_dmx_merger_universe_priority(merger, source, universe_priority);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  // Return the final etcpal_error_t result.
  return result;
}

/**
 * @brief Removes the per-address priority (PAP) data from the source and recalculate outputs.
 *
 * Per-address priority data can time out in sACN just like levels.
 * This is a convenience function to immediately turn off the per-address priority data for a source and recalculate the
 * outputs currently within the valid level count (no merging will occur if levels haven't been inputted yet).
 * Priorities for slots beyond this count will eventually be merged once levels beyond this count are inputted.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source to modify.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_remove_pap(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Validate arguments.
  if (result == kEtcPalErrOk)
  {
    if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
      result = kEtcPalErrNotFound;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      result = remove_sacn_dmx_merger_pap(merger, source);
      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

int merger_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  const MergerState* a = (const MergerState*)value_a;
  const MergerState* b = (const MergerState*)value_b;

  return (a->handle > b->handle) - (a->handle < b->handle);  // Just compare the handles.
}

int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(value_a) || !SACN_ASSERT_VERIFY(value_b))
    return 0;

  const SourceState* a = (const SourceState*)value_a;
  const SourceState* b = (const SourceState*)value_b;

  return (a->handle > b->handle) - (a->handle < b->handle);  // Just compare the handles.
}

EtcPalRbNode* dmx_merger_rb_node_alloc_func(void)
{
  return ALLOC_DMX_MERGER_RB_NODE();
}

void dmx_merger_rb_node_dealloc_func(EtcPalRbNode* node)
{
  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_DMX_MERGER_RB_NODE(node);
}

bool merger_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  return (handle_val == SACN_DMX_MERGER_INVALID) || etcpal_rbtree_find(&mergers, &handle_val);
}

bool source_handle_in_use(int handle_val, void* cookie)
{
  if (!SACN_ASSERT_VERIFY(cookie))
    return false;

  MergerState* merger_state = (MergerState*)cookie;

  return etcpal_rbtree_find(&merger_state->source_state_lookup, &handle_val);
}

// Needs lock
// Use id_to_use as long as it's valid. Otherwise generate the ID with the merger's handle manager.
etcpal_error_t add_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t id_to_use,
                          sacn_dmx_merger_source_t* id_result)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) || !SACN_ASSERT_VERIFY(id_result))
    return kEtcPalErrSys;

  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  size_t source_count_max = SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER;
  sacn_dmx_merger_source_t handle = SACN_DMX_MERGER_SOURCE_INVALID;

  etcpal_error_t state_lookup_insert_result = kEtcPalErrOk;

  // Get the merger state, or return error for invalid handle.
  etcpal_error_t result = lookup_state(merger, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, NULL);

  if (result != kEtcPalErrOk)
    result = kEtcPalErrInvalid;

  // Check if the maximum number of sources has been reached yet.
  if (result == kEtcPalErrOk)
  {
#if SACN_DYNAMIC_MEM
    source_count_max = merger_state->config.source_count_max;
#endif

    if (((source_count_max != SACN_RECEIVER_INFINITE_SOURCES) || !SACN_DYNAMIC_MEM) &&
        (etcpal_rbtree_size(&merger_state->source_state_lookup) >= source_count_max))
    {
      result = kEtcPalErrNoMem;
    }
  }

  if (result == kEtcPalErrOk)
  {
    // Generate a new source handle.
    handle = (id_to_use == SACN_DMX_MERGER_SOURCE_INVALID)
                 ? (sacn_dmx_merger_source_t)get_next_int_handle(&merger_state->source_handle_mgr)
                 : id_to_use;

    // Initialize source state.
    source_state = construct_source_state(handle);

    if (!source_state)
      result = kEtcPalErrNoMem;
  }

  if (result == kEtcPalErrOk)
  {
    state_lookup_insert_result = etcpal_rbtree_insert(&merger_state->source_state_lookup, source_state);

    if (state_lookup_insert_result != kEtcPalErrOk)
    {
      // Clean up and return the correct error.
      FREE_SOURCE_STATE(source_state);

      if (state_lookup_insert_result == kEtcPalErrNoMem)
        result = kEtcPalErrNoMem;
      else
        result = kEtcPalErrSys;
    }
  }

  if (result == kEtcPalErrOk)
    *id_result = handle;

  return result;
}

/*
 * Updates the source levels and recalculates outputs. Assumes all arguments are valid.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_levels, uint16_t new_levels_count)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(new_levels) ||
      !SACN_ASSERT_VERIFY(new_levels_count <= DMX_ADDRESS_COUNT))
  {
    return;
  }

  size_t old_levels_count = source->source.valid_level_count;
  source->source.valid_level_count = new_levels_count;

  if ((new_levels_count != old_levels_count) || (memcmp(new_levels, source->source.levels, new_levels_count) != 0))
  {
    // Copy instead of merging if there's only one source.
    if (etcpal_rbtree_size(&merger->source_state_lookup) == 1)
      update_levels_single_source(merger, source, new_levels, old_levels_count, new_levels_count);
    else
      update_levels_multi_source(merger, source, new_levels, old_levels_count, new_levels_count);
  }
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_pap(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                uint16_t address_priorities_count)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(address_priorities) ||
      !SACN_ASSERT_VERIFY(address_priorities_count <= DMX_ADDRESS_COUNT))
  {
    return;
  }

  size_t old_pap_count = source->pap_count;
  source->pap_count = address_priorities_count;

  if ((address_priorities_count != old_pap_count) ||
      (memcmp(address_priorities, source->source.address_priority, address_priorities_count) != 0))
  {
    SET_PAP_ACTIVE(source);

    if (merger->config.per_address_priorities_active != NULL)
      *(merger->config.per_address_priorities_active) = true;

    // Copy instead of merging if there's only one source.
    if (etcpal_rbtree_size(&merger->source_state_lookup) == 1)
      update_pap_single_source(merger, source, address_priorities, old_pap_count, address_priorities_count);
    else
      update_pap_multi_source(merger, source, address_priorities, old_pap_count, address_priorities_count);
  }
}

/*
 * Updates the source universe priority and recalculates outputs if needed. Assumes all arguments are valid.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source))
    return;

  // There's no need to do anything if the universe priority hasn't changed.
  if ((priority != source->source.universe_priority) || source->universe_priority_uninitialized)
  {
    source->universe_priority_uninitialized = false;

    // Determine if this is the current universe priority output.
    bool was_max = (merger->config.universe_priority != NULL) &&
                   (source->source.universe_priority >= *(merger->config.universe_priority));

    // Also determine if there's only one source or multiple sources in the merger.
    bool single_source = (etcpal_rbtree_size(&merger->source_state_lookup) == 1);

    // Just update the existing entry, since we're not modifying a key.
    source->source.universe_priority = priority;

    // If there are no per-address priorities:
    if (source->source.using_universe_priority)
    {
      // Convert to PAP.
      source->pap_count = DMX_ADDRESS_COUNT;
      uint8_t pap = (priority == 0) ? 1 : priority;

      // Just copy to output if there's only one source, otherwise merge each changed priority.
      if (single_source)
        update_universe_priority_single_source(merger, source, pap);
      else
        update_universe_priority_multi_source(merger, source, pap);
    }

    // Also update the universe priority output if needed.
    if (merger->config.universe_priority != NULL)
    {
      if (single_source || (priority >= *(merger->config.universe_priority)))
        *(merger->config.universe_priority) = priority;
      else if (was_max)  // This used to be the output, but may not be anymore. Recalculate.
        recalculate_universe_priority(merger);
    }
  }
}

/*
 * Copies new levels into the source and the outputs. Assumes all arguments are valid. Assumes there is only one source.
 *
 * Priority and owner outputs will also be updated if the level count changed.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_levels_single_source(MergerState* merger, SourceState* source, const uint8_t* new_levels,
                                 size_t old_levels_count, size_t new_levels_count)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(new_levels) ||
      !SACN_ASSERT_VERIFY(old_levels_count <= DMX_ADDRESS_COUNT) ||
      !SACN_ASSERT_VERIFY(new_levels_count <= DMX_ADDRESS_COUNT))
  {
    return;
  }

  // Update the source state with the current levels.
  memcpy(source->source.levels, new_levels, new_levels_count);

  if (old_levels_count > new_levels_count)
    memset(&source->source.levels[new_levels_count], 0, old_levels_count - new_levels_count);

  // Merge levels. If the level count goes up, merge priorities as well. If it goes down, release slots.
  for (size_t i = 0; i < new_levels_count; ++i)
  {
    if (source->source.address_priority[i] > 0)
      merger->config.levels[i] = new_levels[i];
  }

  if (new_levels_count > old_levels_count)
  {
    for (size_t i = old_levels_count; i < new_levels_count; ++i)
    {
      if (source->source.address_priority[i] > 0)
      {
        merger->config.per_address_priorities[i] = source->source.address_priority[i];
        merger->config.owners[i] = source->handle;
      }
    }
  }

  if (old_levels_count > new_levels_count)
  {
    memset(&merger->config.levels[new_levels_count], 0, old_levels_count - new_levels_count);
    memset(&merger->config.per_address_priorities[new_levels_count], 0, old_levels_count - new_levels_count);
    for (size_t i = new_levels_count; i < old_levels_count; ++i)
      merger->config.owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;
  }
}

/*
 * Updates the source levels and recalculates outputs. Assumes all arguments are valid. Assumes there are multiple
 * sources.
 *
 * Priority and owner outputs will also be updated if the level count changed.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_levels_multi_source(MergerState* merger, SourceState* source, const uint8_t* new_levels,
                                size_t old_levels_count, size_t new_levels_count)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(new_levels) ||
      !SACN_ASSERT_VERIFY(old_levels_count <= DMX_ADDRESS_COUNT) ||
      !SACN_ASSERT_VERIFY(new_levels_count <= DMX_ADDRESS_COUNT))
  {
    return;
  }

  // Update the source state with the current levels.
  memcpy(source->source.levels, new_levels, new_levels_count);

  if (old_levels_count > new_levels_count)
    memset(&source->source.levels[new_levels_count], 0, old_levels_count - new_levels_count);

  // Merge levels. If the level count goes up, merge priorities as well. If it goes down, release slots.
  if (new_levels_count > old_levels_count)
  {
    for (size_t i = 0; i < old_levels_count; ++i)
      merge_new_level(merger, source, i);
    for (size_t i = old_levels_count; i < new_levels_count; ++i)
      merge_new_priority(merger, source, i);  // Priorities were stored in source state, but not yet merged.
  }

  if (old_levels_count >= new_levels_count)
  {
    for (size_t i = 0; i < new_levels_count; ++i)
      merge_new_level(merger, source, i);
    for (size_t i = new_levels_count; i < old_levels_count; ++i)
      merge_new_priority(merger, source, i);  // Causes slots to be released due to reduced level count.
  }
}

/*
 * Copies the new PAP into the source and the outputs. Also updates level and owner outputs. Assumes all arguments are
 * valid. Assumes there is only one source.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_pap_single_source(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                              size_t old_pap_count, size_t new_pap_count)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(address_priorities) ||
      !SACN_ASSERT_VERIFY(old_pap_count <= DMX_ADDRESS_COUNT) ||
      !SACN_ASSERT_VERIFY(new_pap_count <= DMX_ADDRESS_COUNT))
  {
    return;
  }

  // Always track PAP per-source, but only merge priorities for levels that have come in.
  memcpy(source->source.address_priority, address_priorities, new_pap_count);
  if (old_pap_count > new_pap_count)
    memset(&source->source.address_priority[new_pap_count], 0, old_pap_count - new_pap_count);

  memcpy(merger->config.per_address_priorities, source->source.address_priority, source->source.valid_level_count);
  for (size_t i = 0; i < source->source.valid_level_count; ++i)
  {
    if (source->source.address_priority[i] == 0)
    {
      merger->config.levels[i] = 0;
      merger->config.owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;
    }
    else
    {
      merger->config.levels[i] = source->source.levels[i];
      merger->config.owners[i] = source->handle;
    }
  }
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid. Assumes there
 * are multiple sources.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_pap_multi_source(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                             size_t old_pap_count, size_t new_pap_count)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(address_priorities) ||
      !SACN_ASSERT_VERIFY(old_pap_count <= DMX_ADDRESS_COUNT) ||
      !SACN_ASSERT_VERIFY(new_pap_count <= DMX_ADDRESS_COUNT))
  {
    return;
  }

  // Always track PAP per-source, but only merge priorities for levels that have come in.
  memcpy(source->source.address_priority, address_priorities, new_pap_count);
  if (old_pap_count > new_pap_count)
    memset(&source->source.address_priority[new_pap_count], 0, old_pap_count - new_pap_count);

  for (size_t i = 0; i < source->source.valid_level_count; ++i)
    merge_new_priority(merger, source, i);
}

/*
 * Copies the new universe priority (converted to PAP) into the source and the outputs. Also updates level and owner
 * outputs. Assumes all arguments are valid. Assumes there is only one source. Assumes the universe priority changed.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_universe_priority_single_source(MergerState* merger, SourceState* source, uint8_t pap)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source))
    return;

  // Always track PAP per-source, but only merge priorities for levels that have come in.
  memset(source->source.address_priority, pap, DMX_ADDRESS_COUNT);

  memset(merger->config.per_address_priorities, pap, source->source.valid_level_count);
  for (size_t i = 0; i < source->source.valid_level_count; ++i)
    merger->config.owners[i] = source->handle;

  memcpy(merger->config.levels, source->source.levels, source->source.valid_level_count);
}

/*
 * Updates the source universe priority and recalculates outputs if needed. Assumes all arguments are valid. Assumes
 * there are multiple sources. Assumes the universe priority changed.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_universe_priority_multi_source(MergerState* merger, SourceState* source, uint8_t pap)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source))
    return;

  // Always track PAP per-source, but only merge priorities for levels that have come in.
  memset(source->source.address_priority, pap, DMX_ADDRESS_COUNT);
  for (size_t i = 0; i < source->source.valid_level_count; ++i)
    merge_new_priority(merger, source, i);
}

/*
 * Merge a source's new level on a slot. Assumes the priority has not changed since the last merge.
 *
 * This requires sacn_lock to be taken before calling.
 */
void merge_new_level(MergerState* merger, const SourceState* source, size_t slot)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(slot < DMX_ADDRESS_COUNT))
    return;

  // Perform HTP merge when source priority is non-zero and equal to current winning priority.
  if ((source->source.address_priority[slot] > 0) &&
      (source->source.address_priority[slot] == merger->config.per_address_priorities[slot]))
  {
    // Take ownership if source level is greater than current level.
    if (source->source.levels[slot] > merger->config.levels[slot])
    {
      // In merger->config, levels and owners are guaranteed to be non-NULL.
      merger->config.levels[slot] = source->source.levels[slot];
      merger->config.owners[slot] = source->handle;
    }
    // If this source is the current owner and its level decreased, check for a new owner.
    else if ((source->handle == merger->config.owners[slot]) &&
             (source->source.levels[slot] < merger->config.levels[slot]))
    {
      recalculate_winning_level(merger, source, slot);
    }
  }
}

/*
 * Merge a source's new priority on a slot. Assumes the level has not changed since the last merge.
 *
 * This requires sacn_lock to be taken before calling.
 */
void merge_new_priority(MergerState* merger, const SourceState* source, size_t slot)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(slot < DMX_ADDRESS_COUNT))
    return;

  uint8_t source_pap = CALC_SRC_PAP(source, slot);

  // Take ownership if source priority is greater than current priority.
  if (source_pap > merger->config.per_address_priorities[slot])
  {
    // In merger->config, levels and owners and per_address_priorities are guaranteed to be non-NULL.
    merger->config.levels[slot] = source->source.levels[slot];
    merger->config.owners[slot] = source->handle;
    merger->config.per_address_priorities[slot] = source_pap;
  }
  // If this source is not the current owner but has the same priority, take ownership if it has a higher level.
  else if (source->handle != merger->config.owners[slot])
  {
    if ((source_pap > 0) && (source_pap == merger->config.per_address_priorities[slot]) &&
        (source->source.levels[slot] > merger->config.levels[slot]))
    {
      merger->config.levels[slot] = source->source.levels[slot];
      merger->config.owners[slot] = source->handle;
    }
  }
  // If this source is the current owner and its priority decreased, check for a new owner.
  else if (source_pap < merger->config.per_address_priorities[slot])
  {
    recalculate_winning_priority(merger, source, slot);
  }
}

/*
 * Recalculate the winning level on a slot. Assumes the priority has not changed since the last merge.
 *
 * This requires sacn_lock to be taken before calling.
 */
void recalculate_winning_level(MergerState* merger, const SourceState* source, size_t slot)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(slot < DMX_ADDRESS_COUNT))
    return;

  // Start with this source as the owner.
  merger->config.levels[slot] = source->source.levels[slot];

  // Now check if any other sources beat the current source.
  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);
  const SourceState* candidate = etcpal_rbiter_first(&tree_iter, &merger->source_state_lookup);
  do
  {
    if (candidate->handle != source->handle)
    {
      uint8_t candidate_level = candidate->source.levels[slot];

      // Make this source the new owner if it has the same priority and a higher level.
      // Don't need to worry about PAPs beyond level count since candidate_level will be 0.
      if ((candidate->source.address_priority[slot] == merger->config.per_address_priorities[slot]) &&
          (candidate_level > merger->config.levels[slot]))
      {
        merger->config.levels[slot] = candidate_level;
        merger->config.owners[slot] = candidate->handle;
      }
    }
  } while ((candidate = etcpal_rbiter_next(&tree_iter)) != NULL);
}

/*
 * Recalculate the winning priority on a slot. Assumes the level has not changed since the last merge.
 *
 * This requires sacn_lock to be taken before calling.
 */
void recalculate_winning_priority(MergerState* merger, const SourceState* source, size_t slot)
{
  if (!SACN_ASSERT_VERIFY(merger) || !SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(slot < DMX_ADDRESS_COUNT))
    return;

  // Start with this source as the owner.
  merger->config.per_address_priorities[slot] = CALC_SRC_PAP(source, slot);

  // When unsourced, set the level to 0 and owner to invalid.
  if (merger->config.per_address_priorities[slot] == 0)
  {
    merger->config.levels[slot] = 0;
    merger->config.owners[slot] = SACN_DMX_MERGER_SOURCE_INVALID;
  }

  // Now check if any other sources beat the current source.
  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);
  const SourceState* candidate = etcpal_rbiter_first(&tree_iter, &merger->source_state_lookup);
  do
  {
    uint8_t candidate_pap = CALC_SRC_PAP(candidate, slot);

    // Make this source the new owner if it has the same (non-0) priority and a higher level OR a higher priority.
    if ((candidate->handle != source->handle) &&
        ((candidate_pap > merger->config.per_address_priorities[slot]) ||
         ((candidate_pap > 0) && (candidate_pap == merger->config.per_address_priorities[slot]) &&
          (candidate->source.levels[slot] > merger->config.levels[slot]))))
    {
      merger->config.levels[slot] = candidate->source.levels[slot];
      merger->config.owners[slot] = candidate->handle;
      merger->config.per_address_priorities[slot] = candidate_pap;
    }
  } while ((candidate = etcpal_rbiter_next(&tree_iter)) != NULL);
}

/*
 * Recalculate the per_address_priorities_active merger output (assumes it's non-NULL).
 *
 * This requires sacn_lock to be taken before calling.
 */
void recalculate_pap_active(MergerState* merger)
{
  if (!SACN_ASSERT_VERIFY(merger))
    return;

  bool pap_active = false;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  SourceState* source = etcpal_rbiter_first(&iter, &merger->source_state_lookup);
  do
  {
    if (PAP_ACTIVE(source))
      pap_active = true;
  } while (!pap_active && ((source = etcpal_rbiter_next(&iter)) != NULL));

  *(merger->config.per_address_priorities_active) = pap_active;
}

/*
 * Recalculate the universe_priority merger output (assumes it's non-NULL).
 *
 * This requires sacn_lock to be taken before calling.
 */
void recalculate_universe_priority(MergerState* merger)
{
  if (!SACN_ASSERT_VERIFY(merger))
    return;

  uint8_t max_universe_priority = 0;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  SourceState* source = etcpal_rbiter_first(&iter, &merger->source_state_lookup);
  do
  {
    if (source->source.universe_priority > max_universe_priority)
      max_universe_priority = source->source.universe_priority;
  } while ((source = etcpal_rbiter_next(&iter)) != NULL);

  *(merger->config.universe_priority) = max_universe_priority;
}

void free_source_state_lookup_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  FREE_SOURCE_STATE(node->value);
  FREE_DMX_MERGER_RB_NODE(node);
}

void free_mergers_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!SACN_ASSERT_VERIFY(node))
    return;

  MergerState* merger_state = (MergerState*)node->value;

  // Clear the trees within merger state, using callbacks to free memory.
  etcpal_rbtree_clear_with_cb(&merger_state->source_state_lookup, free_source_state_lookup_node);

  // Now free the memory for the merger state and node.
  FREE_MERGER_STATE(merger_state);
  FREE_DMX_MERGER_RB_NODE(node);
}

SourceState* construct_source_state(sacn_dmx_merger_source_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_DMX_MERGER_SOURCE_INVALID))
    return NULL;

  SourceState* source_state = ALLOC_SOURCE_STATE();

  if (source_state)
  {
    source_state->handle = handle;
    source_state->source.id = handle;
    memset(source_state->source.levels, 0, DMX_ADDRESS_COUNT);
    source_state->source.valid_level_count = 0;
    source_state->source.universe_priority = 0;
    source_state->source.using_universe_priority = true;
    memset(source_state->source.address_priority, 0, DMX_ADDRESS_COUNT);
    source_state->pap_count = 0;
    source_state->universe_priority_uninitialized = true;
  }

  return source_state;
}

MergerState* construct_merger_state(sacn_dmx_merger_t handle, const SacnDmxMergerConfig* config)
{
  if (!SACN_ASSERT_VERIFY(config) || !SACN_ASSERT_VERIFY(handle != SACN_DMX_MERGER_INVALID))
    return NULL;

  if (!SACN_ASSERT_VERIFY(config->levels))
    return NULL;

#if SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER
  if (!SACN_ASSERT_VERIFY(config->per_address_priorities))
    return NULL;
#endif

#if SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER
  if (!SACN_ASSERT_VERIFY(config->owners))
    return NULL;
#endif

  MergerState* merger_state = ALLOC_MERGER_STATE();

  if (merger_state)
  {
    // Initialize merger state.
    merger_state->handle = handle;

    const int kMaxValidHandleValue = 0xffff - 1;  // This is the max VALID value, therefore invalid - 1 (0xffff - 1)
    init_int_handle_manager(&merger_state->source_handle_mgr, kMaxValidHandleValue, source_handle_in_use, merger_state);

    etcpal_rbtree_init(&merger_state->source_state_lookup, source_state_lookup_compare_func,
                       dmx_merger_rb_node_alloc_func, dmx_merger_rb_node_dealloc_func);

    merger_state->config = *config;
    memset(merger_state->config.levels, 0, DMX_ADDRESS_COUNT);

#if !SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER
    if (merger_state->config.per_address_priorities == NULL)  // We need to track this - use internal storage.
      merger_state->config.per_address_priorities = merger_state->pap_internal;
#endif

    memset(merger_state->config.per_address_priorities, 0, DMX_ADDRESS_COUNT);

#if !SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER
    if (merger_state->config.owners == NULL)  // We need to track this - use internal storage.
      merger_state->config.owners = merger_state->owners_internal;
#endif

    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      merger_state->config.owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;

    if (merger_state->config.per_address_priorities_active != NULL)
      *(merger_state->config.per_address_priorities_active) = false;
    if (merger_state->config.universe_priority != NULL)
      *(merger_state->config.universe_priority) = 0;
  }

  return merger_state;
}

/*
 * Obtains the state structures for the source and merger specified by the given handles.
 *
 * Keep in mind that merger_state or source_state can be NULL if only interested in one or the other.
 *
 * Call sacn_lock() before using this function or the state data.
 */
etcpal_error_t lookup_state(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, MergerState** merger_state,
                            SourceState** source_state)
{
  etcpal_error_t result = kEtcPalErrOk;

  MergerState* my_merger_state = NULL;
  SourceState* my_source_state = NULL;

  // Look up the merger state.
  my_merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!my_merger_state)
    result = kEtcPalErrNotFound;

  // Look up the source state.
  if ((result == kEtcPalErrOk) && source_state)
  {
    my_source_state = etcpal_rbtree_find(&my_merger_state->source_state_lookup, &source);

    if (!my_source_state)
      result = kEtcPalErrNotFound;
  }

  if (result == kEtcPalErrOk)
  {
    if (merger_state)
      *merger_state = my_merger_state;
    if (source_state)
      *source_state = my_source_state;
  }

  return result;
}

size_t get_number_of_mergers()
{
  size_t result = 0;

  if (sacn_lock())
  {
    result = etcpal_rbtree_size(&mergers);
    sacn_unlock();
  }

  return result;
}

// Needs lock
etcpal_error_t create_sacn_dmx_merger(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle)
{
  if (!SACN_ASSERT_VERIFY(config) || !SACN_ASSERT_VERIFY(handle))
    return kEtcPalErrSys;

  MergerState* merger_state = NULL;
  etcpal_error_t result = kEtcPalErrOk;

  // Allocate merger state.
  merger_state = construct_merger_state(get_next_int_handle(&merger_handle_mgr), config);

  // Verify there was enough memory.
  if (!merger_state)
    result = kEtcPalErrNoMem;

  // Add to the merger tree and verify success.
  if (result == kEtcPalErrOk)
  {
    etcpal_error_t insert_result = etcpal_rbtree_insert(&mergers, merger_state);

    // Verify successful merger tree insertion.
    if (insert_result != kEtcPalErrOk)
    {
      FREE_MERGER_STATE(merger_state);

      if (insert_result == kEtcPalErrNoMem)
        result = kEtcPalErrNoMem;
      else
        result = kEtcPalErrSys;
    }
  }

  // Initialize handle.
  if (result == kEtcPalErrOk)
    *handle = merger_state->handle;

  return result;
}

// Needs lock
etcpal_error_t destroy_sacn_dmx_merger(sacn_dmx_merger_t handle)
{
  if (!SACN_ASSERT_VERIFY(handle != SACN_DMX_MERGER_INVALID))
    return kEtcPalErrSys;

  MergerState* merger_state = NULL;

  // Try to find the merger's state.
  etcpal_error_t result = lookup_state(handle, SACN_DMX_MERGER_SOURCE_INVALID, &merger_state, NULL);

  // Clear the source state lookup tree, using callbacks to free memory.
  if (result == kEtcPalErrOk)
  {
    if (etcpal_rbtree_clear_with_cb(&merger_state->source_state_lookup, free_source_state_lookup_node) != kEtcPalErrOk)
    {
      result = kEtcPalErrSys;
    }
  }

  // Remove from merger tree and free.
  if (result == kEtcPalErrOk)
  {
    if (etcpal_rbtree_remove(&mergers, merger_state) != kEtcPalErrOk)
      result = kEtcPalErrSys;
  }

  if (result == kEtcPalErrOk)
    FREE_MERGER_STATE(merger_state);

  return result;
}

// Needs lock
etcpal_error_t remove_sacn_dmx_merger_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) ||
      !SACN_ASSERT_VERIFY(source != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    return kEtcPalErrSys;
  }

  MergerState* merger_state = NULL;
  SourceState* source_being_removed = NULL;

  // Get the merger and source data, or return invalid if not found.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_being_removed);

  if (result != kEtcPalErrOk)
    result = kEtcPalErrInvalid;

  if (result == kEtcPalErrOk)
  {
    // Merge the source with unsourced priorities to remove this source from the merge output.
    memset(source_being_removed->source.address_priority, 0, DMX_ADDRESS_COUNT);
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
      merge_new_priority(merger_state, source_being_removed, i);

    // Also update universe priority and PAP active outputs if needed.
    if ((merger_state->config.per_address_priorities_active != NULL) &&
        (*(merger_state->config.per_address_priorities_active) == true) && PAP_ACTIVE(source_being_removed))
    {
      SET_PAP_INACTIVE(source_being_removed);
      recalculate_pap_active(merger_state);
    }

    if ((merger_state->config.universe_priority != NULL) &&
        (source_being_removed->source.universe_priority >= *(merger_state->config.universe_priority)))
    {
      source_being_removed->source.universe_priority = 0;
      recalculate_universe_priority(merger_state);
    }

    // Now that the output no longer refers to this source, remove the source from the lookup trees and free its
    // memory.
    if (etcpal_rbtree_remove(&merger_state->source_state_lookup, source_being_removed) != kEtcPalErrOk)
      result = kEtcPalErrSys;
  }

  if (result == kEtcPalErrOk)
    FREE_SOURCE_STATE(source_being_removed);

  return result;
}

// Needs lock
etcpal_error_t add_sacn_dmx_merger_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t* handle)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) || !SACN_ASSERT_VERIFY(handle))
    return kEtcPalErrSys;

  return add_source(merger, SACN_DMX_MERGER_SOURCE_INVALID, handle);
}

// Needs lock
etcpal_error_t add_sacn_dmx_merger_source_with_handle(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t handle_to_use)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) ||
      !SACN_ASSERT_VERIFY(handle_to_use != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    return kEtcPalErrSys;
  }

  sacn_dmx_merger_source_t tmp = SACN_DMX_MERGER_SOURCE_INVALID;
  return add_source(merger, handle_to_use, &tmp);
}

// Needs lock
etcpal_error_t update_sacn_dmx_merger_levels(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                             const uint8_t* new_levels, size_t new_levels_count)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) ||
      !SACN_ASSERT_VERIFY(source != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    return kEtcPalErrSys;
  }

  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  // Look up the merger and source state.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_state);

  // Update this source's level data.
  if ((result == kEtcPalErrOk) && new_levels)
    update_levels(merger_state, source_state, new_levels, (uint16_t)new_levels_count);

  return result;
}

// Needs lock
etcpal_error_t update_sacn_dmx_merger_pap(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source, const uint8_t* pap,
                                          size_t pap_count)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) ||
      !SACN_ASSERT_VERIFY(source != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    return kEtcPalErrSys;
  }

  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  // Look up the merger and source state.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_state);

  // Update this source's per-address-priority data.
  if ((result == kEtcPalErrOk) && pap)
    update_pap(merger_state, source_state, pap, (uint16_t)pap_count);

  return result;
}

// Needs lock
etcpal_error_t update_sacn_dmx_merger_universe_priority(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                        uint8_t universe_priority)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) ||
      !SACN_ASSERT_VERIFY(source != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    return kEtcPalErrSys;
  }

  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  // Look up the merger and source state.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_state);

  // Update this source's universe priority.
  if (result == kEtcPalErrOk)
    update_universe_priority(merger_state, source_state, universe_priority);

  return result;
}

// Needs lock
etcpal_error_t remove_sacn_dmx_merger_pap(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source)
{
  if (!SACN_ASSERT_VERIFY(merger != SACN_DMX_MERGER_INVALID) ||
      !SACN_ASSERT_VERIFY(source != SACN_DMX_MERGER_SOURCE_INVALID))
  {
    return kEtcPalErrSys;
  }

  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  // Look up the merger and source state.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_state);

  if (result == kEtcPalErrOk)
  {
    // Update the using_universe_priority flag.
    bool pap_was_active = PAP_ACTIVE(source_state);
    SET_PAP_INACTIVE(source_state);

    // Merge all the levels again. This time it will use universe priority (converted to PAP).
    memset(source_state->source.address_priority,
           (source_state->source.universe_priority == 0) ? 1 : source_state->source.universe_priority,
           DMX_ADDRESS_COUNT);

    // Only merge priorities for levels that have come in.
    for (size_t i = 0; i < source_state->source.valid_level_count; ++i)
      merge_new_priority(merger_state, source_state, i);

    // Also update the PAP active output if needed.
    if ((merger_state->config.per_address_priorities_active != NULL) && pap_was_active)
      recalculate_pap_active(merger_state);
  }

  return result;
}

#endif  // SACN_DMX_MERGER_ENABLED || DOXYGEN
