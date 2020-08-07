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

// TODO: CLEANUP
//#include <limits.h>
//#include <stdint.h>
//#include <string.h>
//#include "etcpal/acn_pdu.h"
//#include "etcpal/acn_rlp.h"
//#include "etcpal/common.h"
//#include "etcpal/rbtree.h"
//#include "sacn/private/common.h"
//#include "sacn/private/data_loss.h"
//#include "sacn/private/mem.h"
//#include "sacn/private/pdu.h"
//#include "sacn/private/receiver.h"
//#include "sacn/private/sockets.h"
//#include "sacn/private/util.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private constants *****************************/
/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

/* TODO CLEANUP:
#if SACN_DYNAMIC_MEM
#define ALLOC_RECEIVER() malloc(sizeof(SacnReceiver))
#define ALLOC_TRACKED_SOURCE() malloc(sizeof(SacnTrackedSource))
#define FREE_RECEIVER(ptr) free(ptr)
#define FREE_TRACKED_SOURCE(ptr) free(ptr)
#else
#define ALLOC_RECEIVER() etcpal_mempool_alloc(sacnrecv_receivers)
#define ALLOC_TRACKED_SOURCE() etcpal_mempool_alloc(sacnrecv_tracked_sources)
#define FREE_RECEIVER(ptr) etcpal_mempool_free(sacnrecv_receivers, ptr)
#define FREE_TRACKED_SOURCE(ptr) etcpal_mempool_free(sacnrecv_tracked_sources, ptr)
#endif
*/

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
/* TODO: CLEANUP:
ETCPAL_MEMPOOL_DEFINE(sacnrecv_receivers, SacnReceiver, SACN_RECEIVER_MAX_UNIVERSES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_tracked_sources, SacnTrackedSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_rb_nodes, EtcPalRbNode, SACN_RECEIVER_MAX_RB_NODES);
*/
#endif

/*********************** Private function prototypes *************************/



/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN DMX Merger module. Internal function called from sacn_init(). */
etcpal_error_t sacn_dmx_merger_init(void)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.

  // TODO: CLEANUP
  /*
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacnrecv_receivers);
  res |= etcpal_mempool_init(sacnrecv_tracked_sources);
  res |= etcpal_mempool_init(sacnrecv_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&receiver_state.receivers, receiver_compare, node_alloc, node_dealloc);
    etcpal_rbtree_init(&receiver_state.receivers_by_universe, receiver_compare_by_universe, node_alloc, node_dealloc);
    init_int_handle_manager(&receiver_state.handle_mgr, receiver_handle_in_use);
    receiver_state.version_listening = kSacnStandardVersionAll;
    receiver_state.expired_wait = SACN_DEFAULT_EXPIRED_WAIT_MS;
  }
  else
  {
    memset(&receiver_state, 0, sizeof receiver_state);
  }

  return res;
  */
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

  // Check that the merger is added.
  if (!merger_is_added(merger))
  {
    return kEtcPalErrNotFound;
  }

  // Check that the source is added.
  if (sacn_dmx_merger_get_source(merger, source) == NULL)
  {
    return kEtcPalErrNotFound;
  }

  // Update this source's level data.
  result = update_levels(merger, source, new_values, new_values_count);

  // Update this source's universe priority.
  if (result == kEtcPalErrOk)
  {
    result = update_universe_priority(merger, source, priority);
  }

  // Update this source's per-address-priority data.
  if (result == kEtcPalErrOk)
  {
    result = update_per_address_priorities(merger, source, address_priorities, address_priorities_count);
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

  // Check that the merger is added.
  if (!merger_is_added(merger))
  {
    return kEtcPalErrNotFound;
  }

  // Check that the source is added.
  source = sacn_dmx_merger_get_id(merger, &header->cid);
  if (source == SACN_DMX_MERGER_SOURCE_INVALID)
  {
    return kEtcPalErrNotFound;
  }

  // If this is level data (start code 0x00):
  if (header->start_code == 0x00)
  {
    // Update this source's level data.
    result = update_levels(merger, source, pdata, header->slot_count);
  }
  // Else if this is per-address-priority data (start code 0xDD):
  else if (header->start_code == 0xDD)
  {
    // Update this source's per-address-priority data.
    result = update_per_address_priorities(merger, source, pdata, header->slot_count);
  }

  // Update this source's universe priority.
  if (result == kEtcPalErrOk)
  {
    result = update_universe_priority(merger, source, header->priority);
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
  return kEtcPalErrNotImpl;  // TODO: Implement this.
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

/*
 * Updates the source levels and recalculates outputs. Assumes all arguments are valid.
 */
etcpal_error_t update_levels(sacn_dmx_merger_t merger, source_id_t source, const uint8_t* new_values,
                             size_t new_values_count)
{
  etcpal_error_t result = kEtcPalErrOk;

  // For each level:
  for (unsigned int level_index = 0; (result == kEtcPalErrOk) && (level_index < new_values_count); ++level_index)
  {
    // Update the level and recalculate.
    result = update_level(merger, source, level_index, new_values[level_index]);
  }

  // Update the level count
  if (result == kEtcPalErrOk)
  {
    result = update_level_count(merger, source, new_values_count);
  }

  // Return the etcpal_error_t result.
  return result;
}

etcpal_error_t update_level(sacn_dmx_merger_t merger, source_id_t source, unsigned int level_index, uint8_t level)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

etcpal_error_t update_level_count(sacn_dmx_merger_t merger, source_id_t source, size_t new_values_count)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

/*
 * Updates the source per-address-priorities and recalculates outputs. Assumes all arguments are valid.
 */
etcpal_error_t update_per_address_priorities(sacn_dmx_merger_t merger, source_id_t source,
                                             const uint8_t* address_priorities, size_t address_priorities_count)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

/*
 * Updates the source universe priority and recalculates outputs if needed. Assumes all arguments are valid.
 */
etcpal_error_t update_universe_priority(sacn_dmx_merger_t merger, source_id_t source, uint8_t priority)
{
  return kEtcPalErrNotImpl;  // TODO: Implement this.
}

bool merger_is_added(sacn_dmx_merger_t handle)
{
  return false;  // TODO: Implement this.
}
