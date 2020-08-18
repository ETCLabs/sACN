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
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT));
ETCPAL_MEMPOOL_DEFINE(sacnmerge_merger_states, MergerState, SACN_DMX_MERGER_MAX_COUNT);
ETCPAL_MEMPOOL_DEFINE(sacnmerge_rb_nodes, EtcPalRbNode,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT * 2) +
                          SACN_DMX_MERGER_MAX_COUNT);
ETCPAL_MEMPOOL_DEFINE(sacnmerge_cids_to_source_handles, CidToSourceHandle,
                      (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER * SACN_DMX_MERGER_MAX_COUNT));
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
  res |= etcpal_mempool_init(sacnmerge_source_states);
  res |= etcpal_mempool_init(sacnmerge_merger_states);
  res |= etcpal_mempool_init(sacnmerge_rb_nodes);
  res |= etcpal_mempool_init(sacnmerge_cids_to_source_handles);
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
 * \return #kEtcPalErrNoMem: No room to allocate memory for this merger, or the max number of mergers has been reached.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_dmx_merger_create(const SacnDmxMergerConfig* config, sacn_dmx_merger_t* handle)
{
  // Verify module initialized.
  if (!sacn_initialized())
  {
    return kEtcPalErrNotInit;
  }

  // Validate arguments.
  if ((config == NULL) || (handle == NULL) || (config->slots == NULL) || (config->slot_owners == NULL))
  {
    return kEtcPalErrInvalid;
  }

#if !SACN_DYNAMIC_MEM
  // Check if the maximum number of mergers has been reached.
  if (etcpal_rbtree_size(&mergers) >= SACN_DMX_MERGER_MAX_COUNT)
  {
    return kEtcPalErrNoMem;
  }
#endif

  // Allocate merger state.
  MergerState* merger_state = ALLOC_MERGER_STATE();

  // Verify there was enough memory.
  if (merger_state == NULL)
  {
    return kEtcPalErrNoMem;
  }

  // Initialize merger state.
  merger_state->handle = get_next_int_handle(&merger_handle_mgr, -1);
  init_int_handle_manager(&merger_state->source_handle_mgr, source_handle_in_use, merger_state);
  etcpal_rbtree_init(&merger_state->source_state_lookup, source_state_lookup_compare_func,
                     dmx_merger_rb_node_alloc_func, dmx_merger_rb_node_dealloc_func);
  etcpal_rbtree_init(&merger_state->source_handle_lookup, source_handle_lookup_compare_func,
                     dmx_merger_rb_node_alloc_func, dmx_merger_rb_node_dealloc_func);
  merger_state->config = config;
  memset(merger_state->winning_priorities, 0, DMX_ADDRESS_COUNT);
  memset(merger_state->config->slots, 0, DMX_ADDRESS_COUNT);

  for (int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    merger_state->config->slot_owners[i] = SACN_DMX_MERGER_SOURCE_INVALID;
  }

  // Add to the merger tree and verify success.
  etcpal_error_t insert_result = etcpal_rbtree_insert(&mergers, merger_state);

  // Verify successful merger tree insertion.
  if (insert_result != kEtcPalErrOk)
  {
    deinit_merger_state(merger_state);
    FREE_MERGER_STATE(merger_state);

    if (insert_result == kEtcPalErrNoMem)
    {
      return kEtcPalErrNoMem;
    }

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
  // Verify module initialized.
  if (!sacn_initialized())
  {
    return kEtcPalErrNotInit;
  }

  if ((source_cid == NULL) || (source_id == NULL))
  {
    return kEtcPalErrInvalid;
  }

  // Get the merger state, or return error for invalid handle.
  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (merger_state == NULL)
  {
    return kEtcPalErrInvalid;
  }

  // Check if the maximum number of sources has been reached yet.
#if SACN_DYNAMIC_MEM
  size_t source_count_max = merger_state->config->source_count_max;
#else
  size_t source_count_max = SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER;
#endif

  if (source_count_max != SACN_RECEIVER_INFINITE_SOURCES)
  {
    if (etcpal_rbtree_size(&merger_state->source_state_lookup) >= source_count_max)
    {
      return kEtcPalErrNoMem;
    }
  }

  // Generate a new source handle.
  source_id_t handle = (source_id_t)get_next_int_handle(&merger_state->source_handle_mgr, 0xffff);

  // Initialize CID to source handle mapping.
  CidToSourceHandle* cid_to_handle = ALLOC_CID_TO_SOURCE_HANDLE();

  if (cid_to_handle == NULL)
  {
    return kEtcPalErrNoMem;
  }

  memcpy(cid_to_handle->cid.data, source_cid->data, ETCPAL_UUID_BYTES);
  cid_to_handle->handle = handle;

  etcpal_error_t handle_lookup_insert_result = etcpal_rbtree_insert(&merger_state->source_handle_lookup, cid_to_handle);

  if (handle_lookup_insert_result != kEtcPalErrOk)
  {
    // Clean up and return the correct error.
    FREE_CID_TO_SOURCE_HANDLE(cid_to_handle);

    if (handle_lookup_insert_result == kEtcPalErrExists)
    {
      return kEtcPalErrExists;
    }
    else if (handle_lookup_insert_result == kEtcPalErrNoMem)
    {
      return kEtcPalErrNoMem;
    }

    return kEtcPalErrSys;
  }

  // Initialize source state.
  SourceState* source_state = ALLOC_SOURCE_STATE();

  if (source_state == NULL)
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
    {
      return kEtcPalErrNoMem;
    }

    return kEtcPalErrSys;
  }

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
  if (source_cid == NULL)
  {
    return SACN_DMX_MERGER_SOURCE_INVALID;
  }

  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (merger_state == NULL)
  {
    return SACN_DMX_MERGER_SOURCE_INVALID;
  }

  CidToSourceHandle* cid_to_handle = etcpal_rbtree_find(&merger_state->source_handle_lookup, source_cid);

  if (cid_to_handle == NULL)
  {
    return SACN_DMX_MERGER_SOURCE_INVALID;
  }

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
const SacnDmxMergerSource* sacn_dmx_merger_get_source(sacn_dmx_merger_t merger, source_id_t source)
{
  if (source == SACN_DMX_MERGER_SOURCE_INVALID)
  {
    return NULL;
  }

  MergerState* merger_state = etcpal_rbtree_find(&mergers, &merger);

  if (merger_state == NULL)
  {
    return NULL;
  }

  SourceState* source_state = etcpal_rbtree_find(&merger_state->source_state_lookup, &source);

  if (source_state == NULL)
  {
    return NULL;
  }

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
    update_levels(merger_state, source_state, new_values, (uint16_t)new_values_count);
  }

  // Update this source's universe priority.
  update_universe_priority(merger_state, source_state, priority);

  // Update this source's per-address-priority data.
  if (address_priorities != NULL)
  {
    update_per_address_priorities(merger_state, source_state, address_priorities, (uint16_t)address_priorities_count);
  }

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
// TODO: If Receiver API changes to notify both values and per-address priority data in the same callback, this should
// change!!
etcpal_error_t sacn_dmx_merger_update_source_from_sacn(sacn_dmx_merger_t merger, const SacnHeaderData* header,
                                                       const uint8_t* pdata)
{
  source_id_t source = SACN_DMX_MERGER_SOURCE_INVALID;

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
      update_levels(merger_state, source_state, pdata, header->slot_count);
    }
    // Else if this is per-address-priority data (start code 0xDD):
    else if (header->start_code == 0xDD)
    {
      // Update this source's per-address-priority data.
      update_per_address_priorities(merger_state, source_state, pdata, header->slot_count);
    }
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
etcpal_error_t sacn_dmx_merger_stop_source_per_address_priority(sacn_dmx_merger_t merger, source_id_t source)
{
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
  source_state->source.address_priority_valid = false;

  // Merge all the slots again. It will use universe priority this time because address_priority_valid was updated.
  for (uint16_t priority_index = 0; priority_index < DMX_ADDRESS_COUNT; ++priority_index)
  {
    merge_source(merger, source, priority_index);
  }

  return kEtcPalErrOk;
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

int merger_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  const sacn_dmx_merger_t* a = (const sacn_dmx_merger_t*)value_a;
  const sacn_dmx_merger_t* b = (const sacn_dmx_merger_t*)value_b;

  return (*a > *b) - (*a < *b);  // Just compare the handles.
}

int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  const source_id_t* a = (const source_id_t*)value_a;
  const source_id_t* b = (const source_id_t*)value_b;

  return (*a > *b) - (*a < *b);  // Just compare the handles.
}

int source_handle_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  const EtcPalUuid* a = (const EtcPalUuid*)value_a;
  const EtcPalUuid* b = (const EtcPalUuid*)value_b;

  return memcmp(a->data, b->data, ETCPAL_UUID_BYTES);
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

bool source_handle_in_use(int handle_val, void* cookie)
{
  MergerState* merger_state = (MergerState*)cookie;

  return (etcpal_rbtree_find(&merger_state->source_state_lookup, &handle_val) != NULL);
}

/*
 * Updates the source levels and recalculates outputs. Assumes all arguments are valid.
 */
void update_levels(MergerState* merger, SourceState* source, const uint8_t* new_values, uint16_t new_values_count)
{
  // For each level:
  for (uint16_t level_index = 0; level_index < new_values_count; ++level_index)
  {
    // Update the level.
    source->source.values[level_index] = new_values[level_index];
    merge_source(merger, source, level_index);
  }

  // Update the level count
  source->source.valid_value_count = new_values_count;
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid.
 */
void update_per_address_priorities(MergerState* merger, SourceState* source, const uint8_t* address_priorities,
                                   uint16_t address_priorities_count)
{
  // Update the address_priority_valid flag.
  source->source.address_priority_valid = true;

  // For each priority:
  for (uint16_t priority_index = 0; priority_index < address_priorities_count; ++priority_index)
  {
    // Update the priority.
    source->source.address_priority[priority_index] = address_priorities[priority_index];
    merge_source(merger, source, priority_index);
  }
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
    for (uint16_t priority_index = 0; priority_index < DMX_ADDRESS_COUNT; ++priority_index)
    {
      merge_source(merger, source, priority_index);
    }
  }
}

void merge_source(MergerState* merger, SourceState* source, uint16_t slot_index)
{
  uint8_t source_level = source->source.values[slot_index];
  uint8_t source_priority = source->source.address_priority_valid ? source->source.address_priority[slot_index]
                                                                  : source->source.universe_priority;
  source_id_t winning_source = merger->config->slot_owners[slot_index];
  uint8_t winning_level = merger->config->slots[slot_index];
  uint8_t winning_priority = merger->winning_priorities[slot_index];

  // If this source beats the currently winning source:
  if ((winning_source == SACN_DMX_MERGER_SOURCE_INVALID) || (source_priority > winning_priority) ||
      ((source_priority == winning_priority) && (source_level > winning_level)))
  {
    // Replace with this source's data.
    merger->config->slot_owners[slot_index] = source->handle;
    merger->config->slots[slot_index] = source_level;
    merger->winning_priorities[slot_index] = source_priority;
  }
  // Otherwise, if this is the winning source, and it may have lost precedence:
  else if ((winning_source == source->handle) &&
           ((source_priority < winning_priority) || (source_level < winning_level)))
  {
    // Update the winning variables to compare against in the next step.
    winning_level = source_level;
    winning_priority = source_priority;

    // Go through all the sources to find a winner.
    EtcPalRbIter tree_iter;
    etcpal_rbiter_init(&tree_iter);

    SourceState* potential_winner = etcpal_rbiter_first(&tree_iter, &merger->source_state_lookup);

    do
    {
      uint8_t potential_winner_level = potential_winner->source.values[slot_index];
      uint8_t potential_winner_priority = potential_winner->source.address_priority_valid
                                              ? potential_winner->source.address_priority[slot_index]
                                              : potential_winner->source.universe_priority;

      if ((potential_winner_priority > winning_priority) ||
          ((potential_winner_priority == winning_priority) && (potential_winner_level > winning_level)))
      {
        winning_source = potential_winner->handle;
        winning_level = potential_winner_level;
        winning_priority = potential_winner_priority;
      }
    } while ((potential_winner = etcpal_rbiter_next(&tree_iter)) != NULL);

    // Save the final winning values.
    merger->config->slot_owners[slot_index] = winning_source;
    merger->config->slots[slot_index] = winning_level;
    merger->winning_priorities[slot_index] = winning_priority;
  }
}

void deinit_merger_state(MergerState* state)
{
  etcpal_rbtree_clear(&state->source_state_lookup);
  etcpal_rbtree_clear(&state->source_handle_lookup);
}
