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

#define SOURCE_STOPPED_SOURCING(source_state, slot_index)                                                      \
  (source_state->source.address_priority_valid && (source_state->source.address_priority[slot_index] == 0)) || \
      (slot_index >= source_state->source.valid_value_count)

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

#if SACN_DYNAMIC_MEM
#define ALLOC_SOURCE_STATE() malloc(sizeof(SourceState))
#define FREE_SOURCE_STATE(ptr) free(ptr)
#define ALLOC_MERGER_STATE() malloc(sizeof(MergerState))
#define FREE_MERGER_STATE(ptr) free(ptr)
#define ALLOC_DMX_MERGER_RB_NODE() malloc(sizeof(EtcPalRbNode))
#define FREE_DMX_MERGER_RB_NODE(ptr) free(ptr)
#define ALLOC_CID_TO_SOURCE_HANDLE() malloc(sizeof(CidToSourceHandle))
#define FREE_CID_TO_SOURCE_HANDLE(ptr) free(ptr)
#else
#define ALLOC_SOURCE_STATE() etcpal_mempool_alloc(sacnmerge_source_states)
#define FREE_SOURCE_STATE(ptr) etcpal_mempool_free(sacnmerge_source_states, ptr)
#define ALLOC_MERGER_STATE() etcpal_mempool_alloc(sacnmerge_merger_states)
#define FREE_MERGER_STATE(ptr) etcpal_mempool_free(sacnmerge_merger_states, ptr)
#define ALLOC_DMX_MERGER_RB_NODE() etcpal_mempool_alloc(sacnmerge_rb_nodes)
#define FREE_DMX_MERGER_RB_NODE(ptr) etcpal_mempool_free(sacnmerge_rb_nodes, ptr)
#define ALLOC_CID_TO_SOURCE_HANDLE() etcpal_mempool_alloc(sacnmerge_cids_to_source_handles)
#define FREE_CID_TO_SOURCE_HANDLE(ptr) etcpal_mempool_free(sacnmerge_cids_to_source_handles, ptr)
#endif

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacnmerge_source_states, SourceState,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS));
ETCPAL_MEMPOOL_DEFINE(sacnmerge_merger_states, MergerState, SACN_DMX_MERGER_MAX_MERGERS);
ETCPAL_MEMPOOL_DEFINE(sacnmerge_rb_nodes, EtcPalRbNode,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS * 2) +
                          SACN_DMX_MERGER_MAX_MERGERS);
ETCPAL_MEMPOOL_DEFINE(sacnmerge_cids_to_source_handles, CidToSourceHandle,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_MERGERS));
#endif

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN DMX Merger module. Internal function called from sacn_init(). */
etcpal_error_t sacn_dmx_merger_init(void)
{
#if ((SACN_DMX_MERGER_MAX_MERGERS <= 0) || (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER <= 0))
  etcpal_error_t res = kEtcPalErrInvalid;
#else
  etcpal_error_t res = kEtcPalErrOk;
#endif

#if !SACN_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnmerge_source_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnmerge_merger_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnmerge_rb_nodes);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnmerge_cids_to_source_handles);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&mergers, merger_state_lookup_compare_func, dmx_merger_rb_node_alloc_func,
                       dmx_merger_rb_node_dealloc_func);
    init_int_handle_manager(&merger_handle_mgr, merger_handle_in_use, NULL);
  }

  return res;
}

/* Deinitialize the sACN DMX Merger module. Internal function called from sacn_deinit(). */
void sacn_dmx_merger_deinit(void)
{
  etcpal_rbtree_clear_with_cb(&mergers, free_mergers_node);
}

/*!
 * \brief Create a new merger instance.
 *
 * Creates a new merger that uses the passed in config data.  The application owns all buffers
 * in the config, so be sure to call dmx_merger_destroy before destroying the buffers.
 *
 * \param[in] config Configuration parameters for the DMX merger to be created.
 * \param[out] handle Filled in on success with a handle to the merger.
 * \return #kEtcPalErrOk: Merger created successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this merger, or maximum number of mergers has been reached.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_create(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle)
{
  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  // Validate arguments.
  if (!config || !handle || !config->slots || !config->slot_owners)
    return kEtcPalErrInvalid;

  // Allocate merger state.
  MergerState* merger_state = ALLOC_MERGER_STATE();

  // Verify there was enough memory.
  if (!merger_state)
    return kEtcPalErrNoMem;

  // Initialize merger state.
  merger_state->handle = get_next_int_handle(&merger_handle_mgr, -1);
  init_int_handle_manager(&merger_state->source_handle_mgr, source_handle_in_use, merger_state);
  etcpal_rbtree_init(&merger_state->source_state_lookup, source_state_lookup_compare_func,
                     dmx_merger_rb_node_alloc_func, dmx_merger_rb_node_dealloc_func);
  etcpal_rbtree_init(&merger_state->source_handle_lookup, source_handle_lookup_compare_func,
                     dmx_merger_rb_node_alloc_func, dmx_merger_rb_node_dealloc_func);
  merger_state->source_count_max = config->source_count_max;
  merger_state->slots = config->slots;
  merger_state->slot_owners = config->slot_owners;
  memset(merger_state->winning_priorities, 0, DMX_ADDRESS_COUNT);
  memset(merger_state->slots, 0, DMX_ADDRESS_COUNT);

  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
    merger_state->slot_owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;

  // Add to the merger tree and verify success.
  etcpal_error_t insert_result = etcpal_rbtree_insert(&mergers, merger_state);

  // Verify successful merger tree insertion.
  if (insert_result != kEtcPalErrOk)
  {
    FREE_MERGER_STATE(merger_state);

    if (insert_result == kEtcPalErrNoMem)
      return kEtcPalErrNoMem;

    return kEtcPalErrSys;
  }

  // Initialize handle.
  *handle = merger_state->handle;

  return kEtcPalErrOk;
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
  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  // Validate handle.
  if (handle == SACN_DMX_MERGER_INVALID)
    return kEtcPalErrNotFound;

  // Try to find the merger's state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &handle);

  if (!merger_state)
    return kEtcPalErrNotFound;

  // Clear the trees within merger state, using callbacks to free memory.
  if (etcpal_rbtree_clear_with_cb(&merger_state->source_handle_lookup, free_source_handle_lookup_node) != kEtcPalErrOk)
    return kEtcPalErrSys;
  if (etcpal_rbtree_clear_with_cb(&merger_state->source_state_lookup, free_source_state_lookup_node) != kEtcPalErrOk)
    return kEtcPalErrSys;

  // Remove from merger tree and free.
  if (etcpal_rbtree_remove(&mergers, merger_state) != kEtcPalErrOk)
    return kEtcPalErrSys;

  FREE_MERGER_STATE(merger_state);

  return kEtcPalErrOk;
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
                                          sacn_source_id_t* source_id)
{
  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  if (!source_cid || !source_id || (merger == SACN_DMX_MERGER_INVALID))
    return kEtcPalErrInvalid;

  // Get the merger state, or return error for invalid handle.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return kEtcPalErrInvalid;

  // Check if the maximum number of sources has been reached yet.
#if SACN_DYNAMIC_MEM
  size_t source_count_max = merger_state->source_count_max;
#else
  size_t source_count_max = SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER;
#endif

  if (((source_count_max != SACN_RECEIVER_INFINITE_SOURCES) || !SACN_DYNAMIC_MEM) &&
      (etcpal_rbtree_size(&merger_state->source_state_lookup) >= source_count_max))
  {
    return kEtcPalErrNoMem;
  }

  // Generate a new source handle.
  sacn_source_id_t handle = (sacn_source_id_t)get_next_int_handle(&merger_state->source_handle_mgr, 0xffff);

  // Initialize CID to source handle mapping.
  CidToSourceHandle* cid_to_handle = ALLOC_CID_TO_SOURCE_HANDLE();

  if (!cid_to_handle)
    return kEtcPalErrNoMem;

  memcpy(cid_to_handle->cid.data, source_cid->data, ETCPAL_UUID_BYTES);
  cid_to_handle->handle = handle;

  etcpal_error_t handle_lookup_insert_result = etcpal_rbtree_insert(&merger_state->source_handle_lookup, cid_to_handle);

  if (handle_lookup_insert_result != kEtcPalErrOk)
  {
    // Clean up and return the correct error.
    FREE_CID_TO_SOURCE_HANDLE(cid_to_handle);

    if ((handle_lookup_insert_result == kEtcPalErrExists) || (handle_lookup_insert_result == kEtcPalErrNoMem))
      return handle_lookup_insert_result;

    return kEtcPalErrSys;
  }

  // Initialize source state.
  SourceState* source_state = ALLOC_SOURCE_STATE();

  if (!source_state)
  {
    // Clean up and return the correct error.
    etcpal_rbtree_remove(&merger_state->source_handle_lookup, cid_to_handle);
    FREE_CID_TO_SOURCE_HANDLE(cid_to_handle);

    return kEtcPalErrNoMem;
  }

  source_state->handle = handle;
  memcpy(source_state->source.cid.data, source_cid->data, ETCPAL_UUID_BYTES);
  memset(source_state->source.values, 0, DMX_ADDRESS_COUNT);
  source_state->source.valid_value_count = 0;
  source_state->source.universe_priority = 0;
  source_state->source.address_priority_valid = false;
  memset(source_state->source.address_priority, 0, DMX_ADDRESS_COUNT);

  etcpal_error_t state_lookup_insert_result = etcpal_rbtree_insert(&merger_state->source_state_lookup, source_state);

  if (state_lookup_insert_result != kEtcPalErrOk)
  {
    // Clean up and return the correct error.
    etcpal_rbtree_remove(&merger_state->source_handle_lookup, cid_to_handle);
    FREE_CID_TO_SOURCE_HANDLE(cid_to_handle);
    FREE_SOURCE_STATE(source_state);

    if (state_lookup_insert_result == kEtcPalErrNoMem)
      return kEtcPalErrNoMem;

    return kEtcPalErrSys;
  }

  *source_id = handle;

  return kEtcPalErrOk;
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
etcpal_error_t sacn_dmx_merger_remove_source(sacn_dmx_merger_t merger, sacn_source_id_t source)
{
  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  // Check if the handles are invalid.
  if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
    return kEtcPalErrInvalid;

  // Get the merger, or return invalid if not found.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return kEtcPalErrInvalid;

  // Get the source's data, or return an error if not found.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (!source_state)
    return kEtcPalErrInvalid;

  CidToSourceHandle* cid_to_handle = etcpal_rbtree_find(&merger_state->source_handle_lookup, &source_state->source.cid);

  if (!cid_to_handle)
    return kEtcPalErrSys;

  // Merge the source with valid_value_count = 0 to remove this source from the merge output.
  source_state->source.valid_value_count = 0;

  for (uint16_t i = 0; i < DMX_ADDRESS_COUNT; ++i)
    merge_source(merger_state, source_state, i);

  // Now that the output no longer refers to this source, remove the source from the lookup trees and free its memory.
  if (etcpal_rbtree_remove(&merger_state->source_state_lookup, source_state) != kEtcPalErrOk)
    return kEtcPalErrSys;

  FREE_SOURCE_STATE(source_state);

  if (etcpal_rbtree_remove(&merger_state->source_handle_lookup, cid_to_handle) != kEtcPalErrOk)
    return kEtcPalErrSys;

  FREE_CID_TO_SOURCE_HANDLE(cid_to_handle);

  return kEtcPalErrOk;
}

/*!
 * \brief Returns the source id for that source cid.
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source_cid The UUID of the source CID.
 * \return The source ID, or #SACN_DMX_MERGER_SOURCE_INVALID.
 */
sacn_source_id_t sacn_dmx_merger_get_id(sacn_dmx_merger_t merger, const EtcPalUuid* source_cid)
{
  if (!source_cid || (merger == SACN_DMX_MERGER_INVALID))
    return SACN_DMX_MERGER_SOURCE_INVALID;

  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return SACN_DMX_MERGER_SOURCE_INVALID;

  CidToSourceHandle* cid_to_handle = etcpal_rbtree_find(&merger_state->source_handle_lookup, source_cid);

  if (!cid_to_handle)
    return SACN_DMX_MERGER_SOURCE_INVALID;

  return cid_to_handle->handle;
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
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, sacn_source_id_t source)
{
  if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
    return NULL;

  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return NULL;

  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (!source_state)
    return NULL;

  return &source_state->source;
}

/*!
 * \brief Updates the source data and recalculate outputs.
 *
 * The direct method to change source data.  This causes the merger to recalculate the outputs.
 * If you are processing sACN packets, you may prefer dmx_merger_update_source_from_sacn().
 *
 * \param[in] merger The handle to the merger.
 * \param[in] source The id of the source to modify.
 * \param[in] priority The universe-level priority of the source.
 * \param[in] new_values The new DMX values to be copied in. this must be NULL if the source is not updating DMX data.
 * \param[in] new_values_count The length of new_values. Must be 0 if the source is not updating DMX data.
 * \param[in] address_priorities The per-address priority values to be copied in.  This must be NULL if the source is
 * not updating per-address priority data.
 * \param[in] address_priorities_count The length of address_priorities.  Must be 0 if the source is not updating
 * per-address priority data.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_update_source_data(sacn_dmx_merger_t merger, sacn_source_id_t source, uint8_t priority,
                                                  const uint8_t* new_values, size_t new_values_count,
                                                  const uint8_t* address_priorities, size_t address_priorities_count)
{
  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  // Validate arguments.
  if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
    return kEtcPalErrInvalid;
  if (!UNIVERSE_PRIORITY_VALID(priority))
    return kEtcPalErrInvalid;
  if (new_values_count > DMX_ADDRESS_COUNT)
    return kEtcPalErrInvalid;
  if ((new_values && (new_values_count == 0)) || (!new_values && (new_values_count != 0)))
    return kEtcPalErrInvalid;
  if (address_priorities_count > DMX_ADDRESS_COUNT)
    return kEtcPalErrInvalid;
  if ((address_priorities && (address_priorities_count == 0)) ||
      (!address_priorities && (address_priorities_count != 0)))
  {
    return kEtcPalErrInvalid;
  }

  // Look up the merger state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return kEtcPalErrNotFound;

  // Look up the source state.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (!source_state)
    return kEtcPalErrNotFound;

  // Update this source's level data.
  if (new_values)
    update_levels(merger_state, source_state, new_values, (uint16_t)new_values_count);

  // Update this source's universe priority.
  update_universe_priority(merger_state, source_state, priority);

  // Update this source's per-address-priority data.
  if (address_priorities)
    update_per_address_priorities(merger_state, source_state, address_priorities, (uint16_t)address_priorities_count);

  // Return the final etcpal_error_t result.
  return kEtcPalErrOk;
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
etcpal_error_t sacn_dmx_merger_update_source_from_sacn(sacn_dmx_merger_t merger, const SacnHeaderData* header,
                                                       const uint8_t* pdata)
{
  sacn_source_id_t source = SACN_DMX_MERGER_SOURCE_INVALID;

  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  // Validate arguments.
  if ((merger == SACN_DMX_MERGER_INVALID) || !header || (!pdata && (header->slot_count > 0)))
    return kEtcPalErrInvalid;

  if (ETCPAL_UUID_IS_NULL(&header->cid) || !UNIVERSE_ID_VALID(header->universe_id) ||
      !UNIVERSE_PRIORITY_VALID(header->priority) || (header->slot_count > DMX_ADDRESS_COUNT))
  {
    return kEtcPalErrInvalid;
  }

  // Check that the source is added.
  source = sacn_dmx_merger_get_id(merger, &header->cid);

  if (source == SACN_DMX_MERGER_SOURCE_INVALID)
    return kEtcPalErrNotFound;

  // Look up the merger state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return kEtcPalErrNotFound;

  // Look up the source state.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (!source_state)
    return kEtcPalErrNotFound;

  if (pdata)
  {
    // If start_code = 0x00, update level data. Otherwise, if start_code = 0xDD, update per-address priority data.
    if (header->start_code == 0x00)
      update_levels(merger_state, source_state, pdata, header->slot_count);
    else if (header->start_code == 0xDD)
      update_per_address_priorities(merger_state, source_state, pdata, header->slot_count);
  }

  // Update this source's universe priority.
  update_universe_priority(merger_state, source_state, header->priority);

  // Return the final etcpal_error_t result.
  return kEtcPalErrOk;
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
etcpal_error_t sacn_dmx_merger_stop_source_per_address_priority(sacn_dmx_merger_t merger, sacn_source_id_t source)
{
  // Verify module initialized.
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  // Validate arguments.
  if ((merger == SACN_DMX_MERGER_INVALID) || (source == SACN_DMX_MERGER_SOURCE_INVALID))
    return kEtcPalErrNotFound;

  // Look up the merger state.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (!merger_state)
    return kEtcPalErrNotFound;

  // Look up the source state.
  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (!source_state)
    return kEtcPalErrNotFound;

  // Update the address_priority_valid flag.
  source_state->source.address_priority_valid = false;

  // Merge all the slots again. It will use universe priority this time because address_priority_valid was updated.
  for (uint16_t priority_index = 0; priority_index < DMX_ADDRESS_COUNT; ++priority_index)
    merge_source(merger_state, source_state, priority_index);

  return kEtcPalErrOk;
}

int merger_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const sacn_dmx_merger_t* a = (const sacn_dmx_merger_t*)value_a;
  const sacn_dmx_merger_t* b = (const sacn_dmx_merger_t*)value_b;

  return (*a > *b) - (*a < *b);  // Just compare the handles.
}

int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const sacn_source_id_t* a = (const sacn_source_id_t*)value_a;
  const sacn_source_id_t* b = (const sacn_source_id_t*)value_b;

  return (*a > *b) - (*a < *b);  // Just compare the handles.
}

int source_handle_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const EtcPalUuid* a = (const EtcPalUuid*)value_a;
  const EtcPalUuid* b = (const EtcPalUuid*)value_b;

  return memcmp(a->data, b->data, ETCPAL_UUID_BYTES);
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

/*
 * Updates the source levels and recalculates outputs. Assumes all arguments are valid.
 */
void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values, uint16_t new_values_count)
{
  // Update the valid value count.
  source->source.valid_value_count = new_values_count;

  // Update the level values.
  memcpy(source->source.values, new_values, new_values_count);

  // Merge all slots.
  for (uint16_t slot_index = 0; slot_index < DMX_ADDRESS_COUNT; ++slot_index)
    merge_source(merger, source, slot_index);
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid.
 */
void update_per_address_priorities(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                   uint16_t address_priorities_count)
{
  // Update the address_priority_valid flag.
  source->source.address_priority_valid = true;

  // Update the priority values.
  memcpy(source->source.address_priority, address_priorities, address_priorities_count);

  // Any remaining unspecified priorities are interpreted as "not sourced".
  if (address_priorities_count < DMX_ADDRESS_COUNT)
    memset(&source->source.address_priority[address_priorities_count], 0, DMX_ADDRESS_COUNT - address_priorities_count);

  // Merge all slots.
  for (uint16_t slot_index = 0; slot_index < DMX_ADDRESS_COUNT; ++slot_index)
    merge_source(merger, source, slot_index);
}

/*
 * Updates the source universe priority and recalculates outputs if needed. Assumes all arguments are valid.
 */
void update_universe_priority(MergerState* merger, SourceState* source, uint8_t priority)
{
  // Just update the existing entry, since we're not modifying a key.
  source->source.universe_priority = priority;

  // Run the merge now if there are no per-address priorities.
  if (!source->source.address_priority_valid)
  {
    for (uint16_t slot_index = 0; slot_index < DMX_ADDRESS_COUNT; ++slot_index)
      merge_source(merger, source, slot_index);
  }
}

void merge_source(MergerState* merger, SourceState* source, uint16_t slot_index)
{
  uint8_t source_level = source->source.values[slot_index];
  uint8_t source_priority = source->source.address_priority_valid ? source->source.address_priority[slot_index]
                                                                  : source->source.universe_priority;
  bool source_stopped_sourcing = SOURCE_STOPPED_SOURCING(source, slot_index);

  sacn_source_id_t winning_source = merger->slot_owners[slot_index];
  uint8_t winning_level = merger->slots[slot_index];
  uint8_t winning_priority = merger->winning_priorities[slot_index];

  // If this source beats the currently winning source:
  if (!source_stopped_sourcing &&
      ((winning_source == SACN_DMX_MERGER_SOURCE_INVALID) || (source_priority > winning_priority) ||
       ((source_priority == winning_priority) && (source_level > winning_level))))
  {
    // Replace with this source's data.
    merger->slot_owners[slot_index] = source->handle;
    merger->slots[slot_index] = source_level;
    merger->winning_priorities[slot_index] = source_priority;
  }
  // Otherwise, if this is the winning source, and it may have lost precedence:
  else if ((winning_source == source->handle) &&
           (source_stopped_sourcing || (source_priority < winning_priority) || (source_level < winning_level)))
  {
    // Start with this source being the winner, then see if it gets beaten by the other sources (if any).
    winning_level = source_level;
    winning_priority = source_priority;
    bool winner_stopped_sourcing = source_stopped_sourcing;

    // Go through all the sources to see if any can beat the current winner.
    EtcPalRbIter tree_iter;
    etcpal_rbiter_init(&tree_iter);

    SourceState* potential_winner = etcpal_rbiter_first(&tree_iter, &merger->source_state_lookup);

    do
    {
      if (potential_winner->handle != source->handle)  // Don't evaluate the same source.
      {
        uint8_t potential_winner_level = potential_winner->source.values[slot_index];
        uint8_t potential_winner_priority = potential_winner->source.address_priority_valid
                                                ? potential_winner->source.address_priority[slot_index]
                                                : potential_winner->source.universe_priority;
        bool potential_winner_stopped_sourcing = SOURCE_STOPPED_SOURCING(potential_winner, slot_index);

        // If we have a new best:
        if (!potential_winner_stopped_sourcing &&
            (winner_stopped_sourcing || (potential_winner_priority > winning_priority) ||
             ((potential_winner_priority == winning_priority) && (potential_winner_level > winning_level))))
        {
          // Make this the latest winner.
          winning_source = potential_winner->handle;
          winning_level = potential_winner_level;
          winning_priority = potential_winner_priority;
          winner_stopped_sourcing = false;
        }
      }
    } while ((potential_winner = etcpal_rbiter_next(&tree_iter)) != NULL);

    // If the winner is not sourcing:
    if (winner_stopped_sourcing)
    {
      // Indicate that there is no source:
      merger->slot_owners[slot_index] = SACN_DMX_MERGER_SOURCE_INVALID;
      merger->slots[slot_index] = 0;
      merger->winning_priorities[slot_index] = 0;
    }
    else  // Otherwise, save the final winning values.
    {
      merger->slot_owners[slot_index] = winning_source;
      merger->slots[slot_index] = winning_level;
      merger->winning_priorities[slot_index] = winning_priority;
    }
  }
}

void free_source_state_lookup_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SOURCE_STATE(node->value);
  FREE_DMX_MERGER_RB_NODE(node);
}

void free_source_handle_lookup_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_CID_TO_SOURCE_HANDLE(node->value);
  FREE_DMX_MERGER_RB_NODE(node);
}

void free_mergers_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  MergerState* merger_state = (MergerState*)node->value;

  // Clear the trees within merger state, using callbacks to free memory.
  etcpal_rbtree_clear_with_cb(&merger_state->source_handle_lookup, free_source_handle_lookup_node);
  etcpal_rbtree_clear_with_cb(&merger_state->source_state_lookup, free_source_state_lookup_node);

  // Now free the memory for the merger state and node.
  FREE_MERGER_STATE(merger_state);
  FREE_DMX_MERGER_RB_NODE(node);
}
