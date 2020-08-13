/******************************************************************************
 * Copyright 2020 ETC Inc.
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

/***************************** Private constants *****************************/
/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

#if SACN_DYNAMIC_MEM
#define ALLOC_WINNER_LOOKUP_KEYS() malloc(sizeof(WinnerLookupKeys))
#define FREE_WINNER_LOOKUP_KEYS(ptr) free(ptr)
#define ALLOC_SOURCE_STATE() malloc(sizeof(SourceState))
#define FREE_SOURCE_STATE(ptr) free(ptr)
#define ALLOC_MERGER_STATE() malloc(sizeof(MergerState))
#define FREE_MERGER_STATE(ptr) free(ptr)
#define ALLOC_DMX_MERGER_RB_NODE() malloc(sizeof(EtcPalRbNode))
#define FREE_DMX_MERGER_RB_NODE(ptr) free(ptr)
#else
#define ALLOC_WINNER_LOOKUP_KEYS() etcpal_mempool_alloc(sacnmerge_winner_lookup_keys)
#define FREE_WINNER_LOOKUP_KEYS(ptr) etcpal_mempool_free(sacnmerge_winner_lookup_keys, ptr)
#define ALLOC_SOURCE_STATE() etcpal_mempool_alloc(sacnmerge_source_states)
#define FREE_SOURCE_STATE(ptr) etcpal_mempool_free(sacnmerge_source_states, ptr)
#define ALLOC_MERGER_STATE() etcpal_mempool_alloc(sacnmerge_merger_states)
#define FREE_MERGER_STATE(ptr) etcpal_mempool_free(sacnmerge_merger_states, ptr)
#define ALLOC_DMX_MERGER_RB_NODE() etcpal_mempool_alloc(sacnmerge_rb_nodes)
#define FREE_DMX_MERGER_RB_NODE(ptr) etcpal_mempool_free(sacnmerge_rb_nodes, ptr)
#endif

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacnmerge_winner_lookup_keys, WinnerLookupKeys,
                      (DMX_ADDRESS_COUNT * SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT));
ETCPAL_MEMPOOL_DEFINE(sacnmerge_source_states, SourceState,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT));
ETCPAL_MEMPOOL_DEFINE(sacnmerge_merger_states, MergerState, SACN_DMX_MERGER_MAX_COUNT);
ETCPAL_MEMPOOL_DEFINE(sacnmerge_rb_nodes, EtcPalRbNode,
                      (DMX_ADDRESS_COUNT * SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT) +
                          (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT) +
                          SACN_DMX_MERGER_MAX_COUNT);
#endif

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN DMX Merger module. Internal function called from sacn_init(). */
etcpal_error_t sacn_dmx_merger_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacnmerge_winner_lookup_keys);
  res |= etcpal_mempool_init(sacnmerge_source_states);
  res |= etcpal_mempool_init(sacnmerge_merger_states);
  res |= etcpal_mempool_init(sacnmerge_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&mergers, merger_state_compare_func, dmx_merger_rb_node_alloc_func,
                       dmx_merger_rb_node_dealloc_func);
    init_int_handle_manager(&merger_handle_mgr, merger_handle_in_use, NULL);
  }

  return res;
}

/* Deinitialize the sACN DMX Merger module. Internal function called from sacn_deinit(). */
void sacn_dmx_merger_deinit(void)
{
  // TODO: Implement this.

  /*TODO CLEANUP:
  // Clear out the rest of the state tracking
  etcpal_rbtree_clear_with_cb(&receiver_state.receivers, universe_tree_dealloc);
  etcpal_rbtree_clear(&receiver_state.receivers_by_universe);
  memset(&receiver_state, 0, sizeof receiver_state);
  */
}

/*!
 * \brief Create a new merger instance.
 *
 * Creates a new merger that uses the passed in config data.  The application owns all buffers
 * in the config, so be sure to call dmx_merger_destroy before destroying the buffers.
 *
 * \param[in] config Configuration parameters for the DMX merger to be created.
 * \param[out] handle Filled in on success with a handle to the merger.
 * \return #kEtcPalErrOk: Merger created successful.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_create(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

/*!
 * \brief Destroy a merger instance.
 *
 * Tears down the merger and cleans up its resources.
 *
 * \param[in] handle Handle to the merger to destroy.
 * \return #kEtcPalErrOk: Merger destroyed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_destroy(sacn_dmx_merger_t handle)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

/*!
 * \brief Adds a new source to the merger.
 *
 * Adds a new source to the merger, if the maximum number of sources hasn't been reached.
 * The filled in source id is used for two purposes:
 *   - It is the handle for calls that need to access the source data.
 *   - It is the source identifer that is put into the slot_owners buffer that was passed
 *     in the DmxMergerUniverseConfig structure when creating the merger.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source_cid The sACN CID of the source.
 * \param[out] source_id Filled in on success with the source id.
 * \return #kEtcPalErrOk: Source added successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this source, or the max number of sources has been reached.
 * \return #kEtcPalErrExists: the source at that cid was already added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_add_source(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid,
                                          source_id_t* source_id)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

/*!
 * \brief Removes a source from the merger.
 *
 * Removes the source from the merger.  This causes the merger to recalculate the outputs.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source The id of the source to remove.
 * \return #kEtcPalErrOk: Source removed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_remove_source(sacn_dmx_merger_t merger, source_id_t source)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

/*!
 * \brief Returns the source id for that source cid.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source_cid The UUID of the source CID.
 * \return The source ID, or #SACN_DMX_MERGER_SOURCE_INVALID.
 */
source_id_t sacn_dmx_merger_get_id(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid)
{
  return SACN_DMX_MERGER_SOURCE_INVALID;  // TODO: Implement this.
}

/*!
 * \brief Gets a read-only view of the source data.
 *
 * Looks up the source data and returns a pointer to the data or NULL if it doesn't exist.
 * This pointer is owned by the library, and must not be modified by the application.
 * The pointer will only be valid until the source or merger is removed.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source The id of the source.
 * \return The pointer to the source data, or NULL if the source wasn't found.
 */
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, source_id_t source)
{
  return NULL;  // TODO: Implement this.
}

/*!
 * \brief Updates the source data and recalculate outputs.
 *
 * The direct method to change source data.  This causes the merger to recalculate the outputs.
 * If you are processing sACN packets, you may prefer dmx_merger_update_source_from_sacn().
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source The id of the source to modify.
 * \param[in] new_values The new DMX values to be copied in. This must be NULL if the source is only updating the
 * priority or address_priorities.
 * \param[in] new_values_count The length of new_values. Must be 0 if the source is only updating the priority or
 * address_priorities.
 * \param[in] priority The universe-level priority of the source.
 * \param[in] address_priorities The per-address priority values to be copied in.  This must be NULL if the source is
 * not sending per-address priorities, or is only updating other parameters.
 * \param[in] address_priorities_count The length of address_priorities.  Must be 0 if the source is not sending these
 * priorities, or is only updating other parameters.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_update_source_data(sacn_dmx_merger_t merger, source_id_t source,
                                                  const uint8_t* new_values, size_t new_values_count, uint8_t priority,
                                                  const uint8_t* address_priorities, size_t address_priorities_count)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
  {
    return kEtcPalErrNotInit;
  }

  // Validate source.
  if (source == SACN_DMX_MERGER_SOURCE_INVALID)
  {
    return kEtcPalErrInvalid;
  }

  // Validate new_values and new_values_count.
  if (((new_values != NULL) && (new_values_count == 0)) || ((new_values == NULL) && (new_values_count != 0)))
  {
    return kEtcPalErrInvalid;
  }

  if (new_values_count > DMX_ADDRESS_COUNT)
  {
    return kEtcPalErrInvalid;
  }

  // Validate priority.
  if (!UNIVERSE_PRIORITY_VALID(priority))
  {
    return kEtcPalErrInvalid;
  }

  // Validate address_priorities and address_priorities_count.
  if (((address_priorities != NULL) && (address_priorities_count == 0)) ||
      ((address_priorities == NULL) && (address_priorities_count != 0)))
  {
    return kEtcPalErrInvalid;
  }

  if (address_priorities_count > DMX_ADDRESS_COUNT)
  {
    return kEtcPalErrInvalid;
  }

  // Look up the merger state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (merger_state == NULL)
  {
    return kEtcPalErrNotFound;
  }

  // Look up the source state.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (source_state == NULL)
  {
    return kEtcPalErrNotFound;
  }

  // Update this source's level data.
  if (new_values != NULL)
  {
    result = update_levels(merger_state, source_state, new_values, new_values_count);
  }

  // Update this source's universe priority.
  if (result == kEtcPalErrOk)
  {
    result = update_universe_priority(merger_state, source_state, priority);
  }

  // Update this source's per-address-priority data.
  if ((result == kEtcPalErrOk) && (address_priorities != NULL))
  {
    result = update_per_address_priorities(merger_state, source_state, address_priorities, address_priorities_count);
  }

  // Return the final etcpal_error_t result.
  return result;
}

/*!
 * \brief Updates the source data from a sACN packet and recalculate outputs.
 *
 * Processes data passed from the sACN receiver's SacnUniverseDataCallback() handler.  This causes the merger to
 * recalculate the outputs.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] header The sACN header.  Must NOT be NULL.
 * \param[in] pdata The sACN data.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merger, or source CID in the header doesn't match
 * a known source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
// TODO: If Receiver API changes to notify both values and per-address priority data in the same callback, this should
// change!!
etcpal_error_t sacn_dmx_merger_update_source_from_sacn(sacn_dmx_merger_t merger, const SacnHeaderData* header,
                                                       const uint8_t* pdata)
{
  source_id_t source = SACN_DMX_MERGER_SOURCE_INVALID;
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
  {
    return kEtcPalErrNotInit;
  }

  // Validate header.
  if (!header || ETCPAL_UUID_IS_NULL(&header->cid) || !UNIVERSE_ID_VALID(header->universe_id) ||
      !UNIVERSE_PRIORITY_VALID(header->priority))
  {
    return kEtcPalErrInvalid;
  }

  // Validate pdata.
  if ((header->slot_count > 0) && !pdata)
  {
    return kEtcPalErrInvalid;
  }

  // Validate slot count
  if (header->slot_count > DMX_ADDRESS_COUNT)
  {
    return kEtcPalErrInvalid;
  }

  // Check that the source is added.
  source = sacn_dmx_merger_get_id(merger, &header->cid);
  if (source == SACN_DMX_MERGER_SOURCE_INVALID)
  {
    return kEtcPalErrNotFound;
  }

  // Look up the merger state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (merger_state == NULL)
  {
    return kEtcPalErrNotFound;
  }

  // Look up the source state.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (source_state == NULL)
  {
    return kEtcPalErrNotFound;
  }

  if (pdata != NULL)
  {
    // If this is level data (start code 0x00):
    if (header->start_code == 0x00)
    {
      // Update this source's level data.
      result = update_levels(merger_state, source_state, pdata, header->slot_count);
    }
    // Else if this is per-address-priority data (start code 0xDD):
    else if (header->start_code == 0xDD)
    {
      // Update this source's per-address-priority data.
      result = update_per_address_priorities(merger_state, source_state, pdata, header->slot_count);
    }
  }

  // Update this source's universe priority.
  if (result == kEtcPalErrOk)
  {
    result = update_universe_priority(merger_state, source_state, header->priority);
  }

  // Return the final etcpal_error_t result.
  return result;
}

/*!
 * \brief Removes the per-address data from the source and recalculate outputs.
 *
 * Per-address priority data can time out in sACN just like values.
 * This is a convenience function to immediately turn off the per-address priority data for a source and recalculate the
 * outputs.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source The id of the source to modify.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_stop_source_per_address_priority(sacn_dmx_merger_t merger, source_id_t source)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
  {
    return kEtcPalErrNotInit;
  }

  // Validate source.
  if (source == SACN_DMX_MERGER_SOURCE_INVALID)
  {
    return kEtcPalErrInvalid;
  }

  // Look up the merger state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (merger_state == NULL)
  {
    return kEtcPalErrNotFound;
  }

  // Look up the source state.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (source_state == NULL)
  {
    return kEtcPalErrNotFound;
  }

  // Update the address_priority_valid flag.
  source_state->address_priority_valid = false;

  // Reset all priorities to the universe priority.
  for (unsigned int priority_index = 0; (result == kEtcPalErrOk) && (priority_index < DMX_ADDRESS_COUNT);
       ++priority_index)
  {
    // Update the priority.
    result = update_priority(merger, source, priority_index, source_state->universe_priority);
  }

  return result;
}

/*!
 * \brief Fully recalculate outputs.
 *
 * Does a full recalculation of the merger outputs.
 *
 * \param[in] merger The handle to the merger.
 * \return #kEtcPalErrOk: Source updated and recalculation completed.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
// TODO: Do we need this?
etcpal_error_t sacn_dmx_merger_recalculate(sacn_dmx_merger_t merger)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

int merger_state_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  const sacn_dmx_merger_t* a = (const sacn_dmx_merger_t*)value_a;
  const sacn_dmx_merger_t* b = (const sacn_dmx_merger_t*)value_b;

  return (*a > *b) - (*a < *b);  // Just compare the handles.
}

int source_state_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  const source_id_t* a = (const source_id_t*)value_a;
  const source_id_t* b = (const source_id_t*)value_b;

  return (*a > *b) - (*a < *b);  // Just compare the handles.
}

int winner_keys_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  const WinnerLookupKeys* a = (const WinnerLookupKeys*)value_a;
  const WinnerLookupKeys* b = (const WinnerLookupKeys*)value_b;

  // Make sure there is a separate ranking for each slot.
  if (a->slot_index != b->slot_index)
  {
    return (a->slot_index > b->slot_index) - (a->slot_index < b->slot_index);
  }

  // These are the rules for the HTP algorithm. On a given slot, highest priority goes first, followed by highest level.
  if (a->priority != b->priority)
  {
    return (a->priority > b->priority) - (a->priority < b->priority);
  }

  if (a->level != b->level)
  {
    return (a->level > b->level) - (a->level < b->level);
  }

  // If the priorities AND levels are the same, the handle becomes the tiebreaker.
  return (a->owner > b->owner) - (a->owner < b->owner);
}

EtcPalRbNode* dmx_merger_rb_node_alloc_func()
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

  return (etcpal_rbtree_find(&mergers, &handle_val) != NULL);
}

/*
 * Updates the source levels and recalculates outputs. Assumes all arguments are valid.
 */
etcpal_error_t update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values,
                             size_t new_values_count)
{
  etcpal_error_t result = kEtcPalErrOk;

  // For each level:
  for (unsigned int level_index = 0; (result == kEtcPalErrOk) && (level_index < new_values_count); ++level_index)
  {
    // Update the level.
    result = update_level(merger, source, level_index, new_values[level_index]);
  }

  // Update the level count
  if (result == kEtcPalErrOk)
  {
    // Just update the existing entry, since we're not modifying a key.
    source->valid_value_count = new_values_count;
  }

  // Return the etcpal_error_t result.
  return result;
}

etcpal_error_t update_level(MergerState* merger, SourceState* source, unsigned int level_index, uint8_t level)
{
  // Get the current winner lookup keys
  WinnerLookupKeys* current_keys = get_current_keys(merger, source, level_index);

  // Create the new keys
  WinnerLookupKeys new_keys;
  new_keys.slot_index = level_index;
  new_keys.owner = source->handle;
  new_keys.level = level;
  new_keys.priority = (current_keys == NULL) ? 0 : current_keys->priority;

  // Update winner lookup
  etcpal_error_t result = update_winner_lookup(merger, level_index, current_keys, &new_keys);

  if (result == kEtcPalErrOk)
  {
    update_merge(merger, level_index);
  }

  return result;
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid.
 */
etcpal_error_t update_per_address_priorities(MergerState* merger, SourceState* source,
                                             const uint8_t* address_priorities, size_t address_priorities_count)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Update the address_priority_valid flag.
  source->address_priority_valid = true;

  // For each priority:
  for (unsigned int priority_index = 0; (result == kEtcPalErrOk) && (priority_index < address_priorities_count);
       ++priority_index)
  {
    // Update the priority.
    result = update_priority(merger, source, priority_index, address_priorities[priority_index]);
  }

  // Return the etcpal_error_t result.
  return result;
}

etcpal_error_t update_priority(MergerState* merger, SourceState* source, unsigned int priority_index, uint8_t priority)
{
  // Get the current winner lookup keys
  WinnerLookupKeys* current_keys = get_current_keys(merger, source, priority_index);

  // Create the new keys
  WinnerLookupKeys new_keys;
  new_keys.slot_index = priority_index;
  new_keys.owner = source->handle;
  new_keys.level = (current_keys == NULL) ? 0 : current_keys->level;
  new_keys.priority = priority;

  // Update winner lookup
  etcpal_error_t result = update_winner_lookup(merger, priority_index, current_keys, &new_keys);

  if (result == kEtcPalErrOk)
  {
    update_merge(merger, priority_index);
  }

  return result;
}

/*
 * Updates the source universe priority and recalculates outputs if needed. Assumes all arguments are valid.
 */
etcpal_error_t update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Just update the existing entry, since we're not modifying a key.
  source->universe_priority = priority;

  // Update the actual priorities now if there are no per-address priorities.
  if (!source->address_priority_valid)
  {
    for (unsigned int priority_index = 0; (result == kEtcPalErrOk) && (priority_index < DMX_ADDRESS_COUNT);
         ++priority_index)
    {
      // Update the priority.
      result = update_priority(merger, source, priority_index, priority);
    }
  }

  return result;
}

WinnerLookupKeys get_winner_lookup_source_lower_bound_keys(unsigned int slot_index, SourceState* source)
{
  WinnerLookupKeys result;

  result.slot_index = slot_index;
  result.owner = source->handle;
  result.level = 0x00;
  result.priority = 0x00;

  return result;
}

WinnerLookupKeys get_winner_lookup_source_upper_bound_keys(unsigned int slot_index, SourceState* source)
{
  WinnerLookupKeys result;

  result.slot_index = slot_index;
  result.owner = source->handle;
  result.level = 0xff;
  result.priority = 0xff;

  return result;
}

WinnerLookupKeys get_winner_lookup_slot_lower_bound_keys(unsigned int slot_index)
{
  WinnerLookupKeys result;

  result.slot_index = slot_index;
  result.owner = 0x0000;
  result.level = 0x00;
  result.priority = 0x00;

  return result;
}

WinnerLookupKeys get_winner_lookup_slot_upper_bound_keys(unsigned int slot_index)
{
  WinnerLookupKeys result;

  result.slot_index = slot_index;
  result.owner = 0xffff;
  result.level = 0xff;
  result.priority = 0xff;

  return result;
}

etcpal_error_t update_winner_lookup(MergerState* merger, WinnerLookupKeys* current_keys_to_free,
                                    const WinnerLookupKeys* new_keys_to_copy)
{
  etcpal_error_t result = kEtcPalErrOk;

  // If the current keys were found, then remove them from the winner lookup and free them.
  // This is because we are modifying keys, which may change the ordering of the tree.
  if (current_keys_to_free != NULL)
  {
    result = etcpal_rbtree_remove(&merger->winner_lookup, current_keys_to_free);
    FREE_WINNER_LOOKUP_KEYS(current_keys_to_free);
  }

  if (result == kEtcPalErrOk)
  {
    // If the new keys are valid, then add them to the winner lookup.
    if (keys_valid(new_keys_to_copy))
    {
      // Create a heap-allocated copy to use for the lookup tree.
      WinnerLookupKeys* allocated_keys = ALLOC_WINNER_LOOKUP_KEYS();

      if (allocated_keys == NULL)
      {
        result = kEtcPalErrNoMem;
      }
      else
      {
        *allocated_keys = *new_keys_to_copy;
        result = etcpal_rbtree_insert(&merger->winner_lookup, allocated_keys);
      }
    }
  }

  return result;
}

WinnerLookupKeys* get_current_keys(MergerState* merger, SourceState* source, unsigned int slot_index)
{
  WinnerLookupKeys lower_bound_keys = get_winner_lookup_source_lower_bound_keys(slot_index, source);

  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);

  WinnerLookupKeys* result = etcpal_rbiter_lower_bound(&tree_iter, &merger->winner_lookup, &lower_bound_keys);

  if (result != NULL)
  {
    if ((result->slot_index != slot_index) || (result->owner != source->handle))
    {
      result = NULL;
    }
  }

  return result;
}

bool keys_valid(const WinnerLookupKeys* keys)
{
  return (keys != NULL) && (keys->owner != SACN_DMX_MERGER_SOURCE_INVALID);
}

void update_merge(MergerState* merger, unsigned int slot_index)
{
  // Initialize an iterator.
  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);

  // Point the iterator to immediately after the keys for slot_index (if any).
  WinnerLookupKeys upper_bound_keys = get_winner_lookup_slot_upper_bound_keys(slot_index);
  const WinnerLookupKeys* winner_keys = NULL;

  // The highest of the keys for slot_index will be immediately before the upper bound.
  if (etcpal_rbiter_upper_bound(&tree_iter, &merger->winner_lookup, &upper_bound_keys) == NULL)
  {
    winner_keys = etcpal_rbiter_last(&tree_iter, &merger->winner_lookup);
  }
  else
  {
    winner_keys = etcpal_rbiter_prev(&tree_iter);
  }

  // If there is a winner for slot_index, update the slots and slot_owners.
  if ((winner_keys != NULL) && (winner_keys->slot_index == slot_index))
  {
    merger->config->slots[slot_index] = winner_keys->level;
    merger->config->slot_owners[slot_index] = winner_keys->owner;
  }
  else  // Otherwise, slot_owners should indicate that there is no source for this slot.
  {
    merger->config->slot_owners[slot_index] = SACN_DMX_MERGER_SOURCE_INVALID;
  }
}
