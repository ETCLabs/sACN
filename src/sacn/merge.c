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

#include "sacn/merge.h"
#include "sacn/private/merge.h"

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

// TODO: Clean up
// Receiver creation and destruction
// static etcpal_error_t validate_receiver_config(const SacnReceiverConfig* config);
// static SacnReceiver* create_new_receiver(const SacnReceiverConfig* config);
// static void handle_sacn_data_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
// const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr, bool draft);

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Receiver module. Internal function called from sacn_init(). */
etcpal_error_t dmx_merger_init(void)
{
  return kEtcPalErrNotImpl;

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

/* Deinitialize the sACN Receiver module. Internal function called from sacn_deinit(). */
void dmx_merger_deinit(void)
{
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
 * in the config, so be sure to call dmx_merger_destroy_universe before destroying the buffers.
 *
 * \param[in] config Configuration parameters for the DMX merger to be created.
 * \param[out] handle Filled in on success with a handle to the merger.
 * \return #kEtcPalErrOk: Merger created successful.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t dmx_merger_create_universe(const DmxMergerUniverseConfig* config, universe_handle_t* handle)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Destroy an merger instance.
 *
 * Tears down the merger and cleans up its resources.
 *
 * \param[in] handle Handle to the merger to destroy.
 * \return #kEtcPalErrOk: Merger destroyed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t dmx_merger_destroy_universe(universe_handle_t handle)
{
  return kEtcPalErrNotImpl;
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
 * \param[in] universe The handle to the merger.
 * \param[in] source_cid The sACN CID of the source.
 * \param[out] source_id Filled in on success with the source id.
 * \return #kEtcPalErrOk: Source added successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this receiver, or the max number of sources has been
 * reached.
 * \return #kEtcPalErrExists: the source at that cid was already added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t dmx_merger_add_source(universe_handle_t universe, const EtcPalUuid* source_cid, source_id_t* source_id)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Removes a source from the merger.
 *
 * Removes the source from the merger.  This causes the merger to recalculate the outputs.
 *
 * \param[in] universe The handle to the merger.
 * \param[in] source The id of the source to remove.
 * \return #kEtcPalErrOk: Source removed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t dmx_merger_remove_source(universe_handle_t universe, source_id_t source)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Gets a read-only view of the source data.
 *
 * Looks up the source data and returns a pointer to the data or NULL if it doesn't exist.
 * This pointer is owned by the library, and must not be modified by the application.
 * The pointer will only be valid until the source or universe is removed.
 *
 * \param[in] universe The handle to the merger.
 * \param[in] source The id of the source to remove.
 * \return The pointer to the source data, or NULL if the source wasn't found.
 */
const DmxMergerSource* dmx_merger_get_source(universe_handle_t universe, source_id_t source)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Updates the source data and recalculate outputs.
 *
 * The direct method to change source data.  This causes the merger to recalculate the outputs.
 * If you are processing sACN packets, you may prefer dmx_merger_update_source_from_sacn().
 *
 * \param[in] universe The handle to the merger.
 * \param[in] source The id of the source to modify.
 * \param[in] new_values The new DMX values to be copied in.  Must NOT be NULL.
 * \param[in] new_values_count The length of new_values.
 * \param[in] priority The universe-level priority of the source.
 * \param[in] address_priorities The per-address priority values to be copied in.  This may be NULL if the source is not
 * sending per-address priorities.
 * \param[in] address_priorities_count The length of address_priorities.  May be 0 if the source is not sending these
 * priorities.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or universe.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t dmx_merger_update_source_data(universe_handle_t universe, source_id_t source, const uint8_t* new_values,
                                             size_t new_values_count, uint8_t priority,
                                             const uint8_t* address_priorities, size_t address_priorities_count)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Updates the source data from a sACN packet and recalculate outputs.
 *
 * Processes data passed from the sACN receiver's SacnUniverseDataCallback handler.  This causes the merger to
 * recalculate the outputs.
 *
 * \param[in] universe The handle to the merger.
 * \param[in] source The id of the source to modify.
 * \param[in] header The sACN header.  Must NOT be NULL.
 * \param[in] pdata The sACN data.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or universe.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
// TODO: If Receiver API changes to notify both values and per-address priority data in the same callback, this should
// change!!
etcpal_error_t dmx_merger_update_source_from_sacn(universe_handle_t universe, source_id_t source,
                                                  const SacnHeaderData* header, const uint8_t* pdata)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Fully recalculate outputs.
 *
 * Does a full recalculation of the merger outputs.
 *
 * \param[in] universe The handle to the merger.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid universe.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
// TODO: Do we need this?
etcpal_error_t dmx_merger_recalculate(universe_handle_t universe)
{
  return kEtcPalErrNotImpl;
}
