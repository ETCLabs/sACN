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

#define MARK_WINNER_SOURCED(merger_state, slot_index) \
  (merger_state->winner_is_sourced[slot_index / 8] |= (1 << (slot_index % 8)))
#define MARK_WINNER_UNSOURCED(merger_state, slot_index) \
  (merger_state->winner_is_sourced[slot_index / 8] &= ~(1 << (slot_index % 8)))
#define IS_WINNER_SOURCED(merger_state, slot_index) \
  ((merger_state->winner_is_sourced[slot_index / 8] & (1 << (slot_index % 8))) != 0)

/* The next macros define ways to obtain the MergeLevel/MergePriority types. */
#define WINNING_MERGE_LEVEL(merger_state, slot_index) \
  (IS_WINNER_SOURCED(merger_state, slot_index) ? merger_state->config.slots[slot_index] : kUnsourcedLevel)
#define WINNING_MERGE_PRIORITY(merger_state, slot_index)                         \
  ((merger_state->winning_sources[slot_index] == SACN_DMX_MERGER_SOURCE_INVALID) \
       ? kUnsourcedPriority                                                      \
       : merger_state->winning_priorities[slot_index])

#define SOURCE_MERGE_LEVEL(source_state, slot_index) \
  ((slot_index < source_state->source.valid_level_count) ? source_state->source.levels[slot_index] : kUnsourcedLevel)
#define SOURCE_MERGE_PRIORITY(source_state, slot_index)            \
  (source_state->source.address_priority_valid                     \
       ? ((source_state->source.address_priority[slot_index] == 0) \
              ? kUnsourcedPriority                                 \
              : source_state->source.address_priority[slot_index]) \
       : (source_state->has_universe_priority ? source_state->source.universe_priority : kUnsourcedPriority))

/* Determines if the given priority is sourced and winning on the given slot. Comparing directly
 * against winning_priorities (without using WINNING_MERGE_PRIORITY) means that kUnsourcedPriority will always fail. */
#define IS_SOURCED_WINNING_PRIORITY(merge_priority, merger, slot) (merge_priority == merger->winning_priorities[slot])

/* Determines if the given source is actively sourcing the winning priority for the given slot. */
#define IS_SOURCING_WINNING_PRIORITY(source, merger, slot) \
  IS_SOURCED_WINNING_PRIORITY(SOURCE_MERGE_PRIORITY(source, slot), merger, slot)

/* This determines if the given priority is sourced and winning in a similar manner (see IS_SOURCING_WINNING_PRIORITY).
 */

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

/****************************** Private types ********************************/

/* These data types are used to store levels/priorities with the option of an "unsourced" value
 * (kUnsourcedLevel/kUnsourcedPriority). */
typedef int MergeLevel;
typedef int MergePriority;

/**************************** Private constants ******************************/

/* These constants represent a level or priority that is not being sourced. Only used internally with
 * MergeLevel/MergePriority values. These are defined as negative so that they will be lower than all sourced values. */
static const MergeLevel kUnsourcedLevel = -1;
static const MergePriority kUnsourcedPriority = -1;

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacn_pool_merge_source_states, SourceState,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS));
ETCPAL_MEMPOOL_DEFINE(sacn_pool_merge_merger_states, MergerState, SACN_DMX_MERGER_MAX_MERGERS);
ETCPAL_MEMPOOL_DEFINE(sacn_pool_merge_rb_nodes, EtcPalRbNode,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS * 2) +
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

static void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values,
                          uint16_t new_values_count);
static void update_per_address_priorities(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                          uint16_t address_priorities_count);
static void update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority);
static void update_winner_from_new_level(MergerState* merger, const SourceState* winner, MergeLevel new_level,
                                         size_t slot_index);
static void update_winner_from_new_priority(MergerState* merger, const SourceState* winner, MergePriority new_priority,
                                            size_t slot_index);
static void merge_new_level(MergerState* merger, const SourceState* source, size_t slot, MergeLevel source_level);
static void merge_new_universe_priority(MergerState* merger, const SourceState* source);
static void merge_new_priority(MergerState* merger, const SourceState* source, size_t slot,
                               MergePriority source_priority);
static void recalculate_pap_active_and_universe_priority(MergerState* merger, sacn_dmx_merger_source_t skip_this);

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
    if (!config || !handle || !config->slots)
      result = kEtcPalErrInvalid;
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
 *   - It is the source identifer that is put into the slot_owners buffer that was passed
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
 * This function updates the levels of the specified source, and then triggers the recalculation of each slot. For each
 * slot, the source will only be included in the merge if it has a level and a priority at that slot.
 *
 * @param[in] merger The handle to the merger.
 * @param[in] source The id of the source to modify.
 * @param[in] new_levels The new DMX levels to be copied in, starting from the first slot.
 * @param[in] new_levels_count The length of new_levels.
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
 * of each slot. For each slot, the source will only be included in the merge if it has a level and a priority at that
 * slot.
 *
 * If PAP is not specified for all slots, then the remaining slots will default to a PAP of 0. To remove PAP for this
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
 * slot. For each slot, the source will only be included in the merge if it has a level and a priority at that slot.
 *
 * If per-address priorities (PAP) were previously specified for this source with sacn_dmx_merger_update_pap, then the
 * universe priority can have no effect on the merge results until the application calls sacn_dmx_merger_remove_pap, at
 * which point the priorities of each slot will revert to the universe priority passed in here.
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
 * outputs.
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

  const MergerState* a = (const MergerState*)value_a;
  const MergerState* b = (const MergerState*)value_b;

  return (a->handle > b->handle) - (a->handle < b->handle);  // Just compare the handles.
}

int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

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
  FREE_DMX_MERGER_RB_NODE(node);
}

bool merger_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  return (handle_val == SACN_DMX_MERGER_INVALID) || etcpal_rbtree_find(&mergers, &handle_val);
}

bool source_handle_in_use(int handle_val, void* cookie)
{
  MergerState* merger_state = (MergerState*)cookie;

  return (handle_val == SACN_DMX_MERGER_SOURCE_INVALID) ||
         etcpal_rbtree_find(&merger_state->source_state_lookup, &handle_val);
}

// Needs lock
// Use id_to_use as long as it's valid. Otherwise generate the ID with the merger's handle manager.
etcpal_error_t add_source(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t id_to_use,
                          sacn_dmx_merger_source_t* id_result)
{
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
void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values, uint16_t new_values_count)
{
  size_t old_values_count = source->source.valid_level_count;
  source->source.valid_level_count = new_values_count;

  size_t slot = 0;

  // Update and merge *changed* sourced levels that were sourced before.
  while ((slot < old_values_count) && (slot < new_values_count))
  {
    if (new_values[slot] != source->source.levels[slot])
    {
      source->source.levels[slot] = new_values[slot];
      merge_new_level(merger, source, slot, new_values[slot]);
    }

    ++slot;
  }

  // Update and merge all sourced levels that were unsourced before.
  while (slot < new_values_count)
  {
    source->source.levels[slot] = new_values[slot];
    merge_new_level(merger, source, slot, new_values[slot]);

    ++slot;
  }

  // Finally, update and merge all unsourced levels that were sourced before.
  while (slot < old_values_count)
  {
    source->source.levels[slot] = 0;
    merge_new_level(merger, source, slot, kUnsourcedLevel);

    ++slot;
  }
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_per_address_priorities(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                   uint16_t address_priorities_count)
{
  // Update the relevant flags.
  bool pap_was_invalid = !source->source.address_priority_valid;
  source->source.address_priority_valid = true;

  if (merger->config.per_address_priorities_active != NULL)
    *merger->config.per_address_priorities_active = true;

  // Update and merge priorities that have changed.
  for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    uint8_t new_pap = (i < address_priorities_count) ? address_priorities[i] : 0;

    // If this priority has changed:
    if (pap_was_invalid || (new_pap != source->source.address_priority[i]))
    {
      // Update the source data.
      source->source.address_priority[i] = new_pap;

      // Run the merge.
      merge_new_priority(merger, source, i, (new_pap == 0) ? kUnsourcedPriority : new_pap);
    }
  }
}

/*
 * Updates the source universe priority and recalculates outputs if needed. Assumes all arguments are valid.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority)
{
  // If the universe priority actually changed:
  if (!source->has_universe_priority || (source->source.universe_priority != priority))
  {
    // Determine if this is the current universe priority output.
    bool was_max = (merger->config.universe_priority != NULL) && source->has_universe_priority &&
                   (source->source.universe_priority >= *merger->config.universe_priority);

    // Just update the existing entry, since we're not modifying a key.
    source->source.universe_priority = priority;
    source->has_universe_priority = true;

    // Run the merge now if there are no per-address priorities.
    if (!source->source.address_priority_valid)
      merge_new_universe_priority(merger, source);

    // Also update the universe priority output if needed.
    if (merger->config.universe_priority != NULL)
    {
      if (priority >= *merger->config.universe_priority)
        *merger->config.universe_priority = priority;
      else if (was_max)  // This used to be the output, but may not be anymore. Recalculate.
        recalculate_pap_active_and_universe_priority(merger, SACN_DMX_MERGER_SOURCE_INVALID);
    }
  }
}

/*
 * Updates newly determined winning merge values caused by a new level. Assumes the priority at the given slot is being
 * sourced.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_winner_from_new_level(MergerState* merger, const SourceState* winner, MergeLevel new_level,
                                  size_t slot_index)
{
  if (new_level == kUnsourcedLevel)
  {
    MARK_WINNER_UNSOURCED(merger, slot_index);

    merger->config.slots[slot_index] = 0;

    if (merger->config.slot_owners)
      merger->config.slot_owners[slot_index] = SACN_DMX_MERGER_SOURCE_INVALID;

    if (merger->config.per_address_priorities)
      merger->config.per_address_priorities[slot_index] = 0;
  }
  else
  {
    MARK_WINNER_SOURCED(merger, slot_index);

    merger->config.slots[slot_index] = (uint8_t)new_level;

    if (merger->config.slot_owners)
      merger->config.slot_owners[slot_index] = winner->handle;

    if (merger->config.per_address_priorities)
    {
      /* The winning priority at this slot must be sourced at this point, but it might not have been written to the
       * config output yet (i.e. when the level was previously unsourced), so do that now. */
      uint8_t winning_priority = merger->winning_priorities[slot_index];
      merger->config.per_address_priorities[slot_index] = (winning_priority == 0) ? 1 : winning_priority;
    }
  }

  // All that winning_sources requires is a winning priority - it doesn't matter if the level is unsourced.
  merger->winning_sources[slot_index] = winner->handle;
}

/*
 * Updates newly determined winning merge values caused by a new priority.
 *
 * This requires sacn_lock to be taken before calling.
 */
void update_winner_from_new_priority(MergerState* merger, const SourceState* winner, MergePriority new_priority,
                                     size_t slot_index)
{
  if (new_priority == kUnsourcedPriority)
  {
    merger->winning_priorities[slot_index] = 0;
    merger->winning_sources[slot_index] = SACN_DMX_MERGER_SOURCE_INVALID;
  }
  else
  {
    // Even if the level is not sourced, the source should be tracked as a winner (internally).
    merger->winning_priorities[slot_index] = (uint8_t)new_priority;
    merger->winning_sources[slot_index] = winner->handle;
  }

  if ((new_priority == kUnsourcedPriority) || (slot_index >= winner->source.valid_level_count))
  {
    MARK_WINNER_UNSOURCED(merger, slot_index);

    merger->config.slots[slot_index] = 0;

    if (merger->config.per_address_priorities)
      merger->config.per_address_priorities[slot_index] = 0;

    if (merger->config.slot_owners)
      merger->config.slot_owners[slot_index] = SACN_DMX_MERGER_SOURCE_INVALID;
  }
  else
  {
    MARK_WINNER_SOURCED(merger, slot_index);

    merger->config.slots[slot_index] = winner->source.levels[slot_index];

    if (merger->config.per_address_priorities)
      merger->config.per_address_priorities[slot_index] = (new_priority == 0) ? 1 : (uint8_t)new_priority;

    if (merger->config.slot_owners)
      merger->config.slot_owners[slot_index] = winner->handle;
  }
}

/*
 * Merge a source's new level on a slot.
 *
 * This requires sacn_lock to be taken before calling.
 */
void merge_new_level(MergerState* merger, const SourceState* source, size_t slot, MergeLevel source_level)
{
  if (IS_SOURCING_WINNING_PRIORITY(source, merger, slot))
  {
    MergeLevel current_winning_level = WINNING_MERGE_LEVEL(merger, slot);

    if (source_level > current_winning_level)
    {
      update_winner_from_new_level(merger, source, source_level, slot);
    }
    else if ((source->handle == merger->winning_sources[slot]) && (source_level < current_winning_level))
    {
      MergeLevel highest_level = source_level;
      const SourceState* winner = source;

      EtcPalRbIter tree_iter;
      etcpal_rbiter_init(&tree_iter);
      const SourceState* src = etcpal_rbiter_first(&tree_iter, &merger->source_state_lookup);
      do
      {
        if ((src->handle != source->handle) && IS_SOURCING_WINNING_PRIORITY(src, merger, slot) &&
            (SOURCE_MERGE_LEVEL(src, slot) > highest_level))
        {
          highest_level = src->source.levels[slot];
          winner = src;
        }
      } while ((src = etcpal_rbiter_next(&tree_iter)) != NULL);

      // Save the final winning values.
      update_winner_from_new_level(merger, winner, highest_level, slot);
    }
  }
}

/*
 * Merge a source's new universe priority on all slots. Only called when PAP is invalid.
 *
 * Universe priority might not be present yet, such as when removing PAP when no universe priority was ever specified.
 *
 * This requires sacn_lock to be taken before calling.
 */
void merge_new_universe_priority(MergerState* merger, const SourceState* source)
{
  if (source->has_universe_priority)
  {
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
      merge_new_priority(merger, source, i, source->source.universe_priority);
  }
  else
  {
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
      merge_new_priority(merger, source, i, kUnsourcedPriority);
  }
}

/*
 * Merge a source's new priority on a slot. This can be used for both PAP and universe priorities.
 *
 * If source_priority is a PAP, then 0 should be translated to kUnsourcedPriority by the caller.
 *
 * This requires sacn_lock to be taken before calling.
 */
void merge_new_priority(MergerState* merger, const SourceState* source, size_t slot, MergePriority source_priority)
{
  MergePriority current_winning_priority = WINNING_MERGE_PRIORITY(merger, slot);

  if (source_priority > current_winning_priority)
  {
    update_winner_from_new_priority(merger, source, source_priority, slot);
  }
  else if (source->handle != merger->winning_sources[slot])
  {
    if (IS_SOURCED_WINNING_PRIORITY(source_priority, merger, slot) &&
        (SOURCE_MERGE_LEVEL(source, slot) > WINNING_MERGE_LEVEL(merger, slot)))
    {
      update_winner_from_new_priority(merger, source, source_priority, slot);
    }
  }
  else  // source->handle == merger->winning_sources[i]
  {
    if (source_priority < current_winning_priority)
    {
      MergePriority highest_priority = source_priority;
      MergeLevel winner_level = SOURCE_MERGE_LEVEL(source, slot);
      const SourceState* winner = source;

      EtcPalRbIter tree_iter;
      etcpal_rbiter_init(&tree_iter);
      const SourceState* src = etcpal_rbiter_first(&tree_iter, &merger->source_state_lookup);
      do
      {
        if (src->handle != source->handle)
        {
          MergeLevel src_level = SOURCE_MERGE_LEVEL(src, slot);
          MergePriority src_priority = SOURCE_MERGE_PRIORITY(src, slot);

          if ((src_priority > highest_priority) || ((src_priority == highest_priority) && (src_level > winner_level)))
          {
            highest_priority = src_priority;
            winner_level = src_level;
            winner = src;
          }
        }
      } while ((src = etcpal_rbiter_next(&tree_iter)) != NULL);

      // Save the final winning values.
      update_winner_from_new_priority(merger, winner, highest_priority, slot);
    }
  }
}

/*
 * Recalculate the per_address_priorities_active and universe_priority merger outputs.
 *
 * This requires sacn_lock to be taken before calling.
 */
void recalculate_pap_active_and_universe_priority(MergerState* merger, sacn_dmx_merger_source_t skip_this)
{
  bool pap_active = false;
  uint8_t max_universe_priority = 0;

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  SourceState* source = etcpal_rbiter_first(&iter, &merger->source_state_lookup);
  do
  {
    if (source->handle != skip_this)
    {
      if (source->source.address_priority_valid)
        pap_active = true;
      if (source->has_universe_priority && (source->source.universe_priority > max_universe_priority))
        max_universe_priority = source->source.universe_priority;
    }
  } while ((source = etcpal_rbiter_next(&iter)) != NULL);

  if (merger->config.per_address_priorities_active != NULL)
    *merger->config.per_address_priorities_active = pap_active;
  if (merger->config.universe_priority != NULL)
    *merger->config.universe_priority = max_universe_priority;
}

void free_source_state_lookup_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SOURCE_STATE(node->value);
  FREE_DMX_MERGER_RB_NODE(node);
}

void free_mergers_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  MergerState* merger_state = (MergerState*)node->value;

  // Clear the trees within merger state, using callbacks to free memory.
  etcpal_rbtree_clear_with_cb(&merger_state->source_state_lookup, free_source_state_lookup_node);

  // Now free the memory for the merger state and node.
  FREE_MERGER_STATE(merger_state);
  FREE_DMX_MERGER_RB_NODE(node);
}

SourceState* construct_source_state(sacn_dmx_merger_source_t handle)
{
  SourceState* source_state = ALLOC_SOURCE_STATE();

  if (source_state)
  {
    source_state->handle = handle;
    source_state->source.id = handle;
    memset(source_state->source.levels, 0, DMX_ADDRESS_COUNT);
    source_state->source.valid_level_count = 0;
    source_state->source.universe_priority = 0;
    source_state->source.address_priority_valid = false;
    memset(source_state->source.address_priority, 0, DMX_ADDRESS_COUNT);
    source_state->has_universe_priority = false;
  }

  return source_state;
}

MergerState* construct_merger_state(sacn_dmx_merger_t handle, const SacnDmxMergerConfig* config)
{
  MergerState* merger_state = ALLOC_MERGER_STATE();

  if (merger_state)
  {
    // Initialize merger state.
    merger_state->handle = handle;
    init_int_handle_manager(&merger_state->source_handle_mgr, 0xffff, source_handle_in_use, merger_state);

    etcpal_rbtree_init(&merger_state->source_state_lookup, source_state_lookup_compare_func,
                       dmx_merger_rb_node_alloc_func, dmx_merger_rb_node_dealloc_func);

    merger_state->config = *config;
    memset(merger_state->config.slots, 0, DMX_ADDRESS_COUNT);

    if (merger_state->config.per_address_priorities)
      memset(merger_state->config.per_address_priorities, 0, DMX_ADDRESS_COUNT);

    if (merger_state->config.per_address_priorities_active != NULL)
      *merger_state->config.per_address_priorities_active = false;
    if (merger_state->config.universe_priority != NULL)
      *merger_state->config.universe_priority = 0;

    if (merger_state->config.slot_owners)
    {
      for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
        merger_state->config.slot_owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;
    }

    memset(merger_state->winning_priorities, 0, DMX_ADDRESS_COUNT);
    for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
      merger_state->winning_sources[i] = SACN_DMX_MERGER_SOURCE_INVALID;

    memset(merger_state->winner_is_sourced, 0, DMX_ADDRESS_COUNT / 8);
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
  MergerState* merger_state = NULL;
  SourceState* source_being_removed = NULL;

  // Get the merger and source data, or return invalid if not found.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_being_removed);

  if (result != kEtcPalErrOk)
    result = kEtcPalErrInvalid;

  if (result == kEtcPalErrOk)
  {
    // Merge the source with unsourced priorities to remove this source from the merge output.
    for (size_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
      merge_new_priority(merger_state, source_being_removed, i, kUnsourcedPriority);

    // Also update universe priority and PAP active outputs if needed.
    if (((merger_state->config.per_address_priorities_active != NULL) &&
         (*merger_state->config.per_address_priorities_active == true) &&
         (source_being_removed->source.address_priority_valid)) ||
        ((merger_state->config.universe_priority != NULL) &&
         (source_being_removed->source.universe_priority >= *merger_state->config.universe_priority)))
    {
      recalculate_pap_active_and_universe_priority(merger_state, source_being_removed->handle);
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
  return add_source(merger, SACN_DMX_MERGER_SOURCE_INVALID, handle);
}

// Needs lock
etcpal_error_t add_sacn_dmx_merger_source_with_handle(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t handle_to_use)
{
  sacn_dmx_merger_source_t tmp = SACN_DMX_MERGER_SOURCE_INVALID;
  return add_source(merger, handle_to_use, &tmp);
}

// Needs lock
etcpal_error_t update_sacn_dmx_merger_levels(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                             const uint8_t* new_levels, size_t new_levels_count)
{
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
  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  // Look up the merger and source state.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_state);

  // Update this source's per-address-priority data.
  if ((result == kEtcPalErrOk) && pap)
    update_per_address_priorities(merger_state, source_state, pap, (uint16_t)pap_count);

  return result;
}

// Needs lock
etcpal_error_t update_sacn_dmx_merger_universe_priority(sacn_dmx_merger_t merger, sacn_dmx_merger_source_t source,
                                                        uint8_t universe_priority)
{
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
  MergerState* merger_state = NULL;
  SourceState* source_state = NULL;

  // Look up the merger and source state.
  etcpal_error_t result = lookup_state(merger, source, &merger_state, &source_state);

  if (result == kEtcPalErrOk)
  {
    // Update the address_priority_valid flag.
    bool was_valid = source_state->source.address_priority_valid;
    source_state->source.address_priority_valid = false;

    // Merge all the slots again. It will use universe priority this time.
    merge_new_universe_priority(merger_state, source_state);

    // Also update the PAP active output if needed.
    if ((merger_state->config.per_address_priorities_active != NULL) && was_valid)
      recalculate_pap_active_and_universe_priority(merger_state, SACN_DMX_MERGER_SOURCE_INVALID);
  }

  return result;
}

#endif  // SACN_DMX_MERGER_ENABLED || DOXYGEN
