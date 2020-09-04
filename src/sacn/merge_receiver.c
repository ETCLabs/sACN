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

/*********** CHRISTIAN's BIG OL' TODO LIST: *************************************
 - Fill in all the holes I made... :)
 - Make sure everything works with static & dynamic memory. C & C++, too..
 - This entire project should build without warnings!!
*/

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "sacn/private/mem.h"
#include "sacn/private/util.h"
#include "sacn/private/merge_receiver.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private constants *****************************/

/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if SACN_DYNAMIC_MEM
#define ALLOC_MERGE_RECEIVER() malloc(sizeof(SacnMergeReceiver))
#define FREE_MERGE_RECEIVER(ptr) \
  do                       \
  {                        \
    if (ptr->netints)      \
    {                      \
      free(ptr->netints);  \
    }                      \
    free(ptr);             \
  } while (0)
#else
#define ALLOC_MERGE_RECEIVER() etcpal_mempool_alloc(sacnrecv_merge_receivers)
#define FREE_MERGE_RECEIVER(ptr) etcpal_mempool_free(sacnrecv_merge_receivers, ptr)
#endif

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacnrecv_merge_receivers, SacnMergeReceiver, SACN_RECEIVER_MAX_UNIVERSES);
#endif

/*********************** Private function prototypes *************************/

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Merge Receiver module. Internal function called from sacn_init(). */
etcpal_error_t sacn_merge_receiver_init(void)
{
  /*
  //CHRISTIAN TODO CLEANUP
  // TODO: CLEANUP  -- Be sure to check SACN_RECEIVER_MAX_UNIVERSES, as it is illegal to declare a 0-size array in C.

  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacnrecv_receivers);
  res |= etcpal_mempool_init(sacnrecv_tracked_sources);
  res |= etcpal_mempool_init(sacnrecv_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
  }
  else
  {
    memset(&receiver_state, 0, sizeof receiver_state);
  }

  return res;
  */
  return kEtcPalErrNotImpl;
}

/* Deinitialize the sACN Merge Receiver module. Internal function called from sacn_deinit(). */
void sacn_merge_receiver_deinit(void)
{
  /*
  CHRISTIAN TODO CLEANUP

  // Stop all receive threads
  for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
  {
    SacnRecvThreadContext* thread_context = get_recv_thread_context(i);
    if (thread_context && thread_context->running)
    {
      thread_context->running = false;
      etcpal_thread_join(&thread_context->thread_handle);
      sacn_cleanup_dead_sockets(thread_context);
    }
  }

  // Clear out the rest of the state tracking
  etcpal_rbtree_clear_with_cb(&receiver_state.receivers, universe_tree_dealloc);
  etcpal_rbtree_clear(&receiver_state.receivers_by_universe);
  memset(&receiver_state, 0, sizeof receiver_state);
  */
}

/*!
 * \brief Initialize an sACN Merge Receiver Config struct to default values.
 *
 * \param[out] config Config struct to initialize.
 */
void sacn_merge_receiver_config_init(SacnMergeReceiverConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(SacnMergeReceiverConfig));
  }
}

/*!
 * \brief Create a new sACN Merge Receiver to listen and merge sACN data on a universe.
 *
 * An sACN merge receiver can listen on one universe at a time, and each universe can only be listened to
 * by one merge receiver at at time.
 *
 * \param[in] config Configuration parameters for the sACN Merge Receiver to be created.
 * \param[out] handle Filled in on success with a handle to the sACN Merge Receiver.
 * \return #kEtcPalErrOk: Merge Receiver created successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified universe.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this merge receiver, or maximum merge receivers reached.
 * \return #kEtcPalErrNoNetints: No network interfaces were found on the system.
 * \return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_create(const SacnMergeReceiverConfig* config, sacn_merge_receiver_t* handle)
{
  // CHRISTIAN TODO
  if (!config || !handle)
    return kEtcPalErrInvalid;

  return kEtcPalErrNotImpl;
}

/*!
 * \brief Destroy a sACN Merge Receiver instance.
 *
 * \param[in] handle Handle to the merge receiver to destroy.
 * \return #kEtcPalErrOk: Merge receiver destroyed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_destroy(sacn_merge_receiver_t handle)
{
  // CHRISTIAN TODO
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Get the universe on which a sACN Merge Receiver is currently listening.
 *
 * \param[in] handle Handle to the merge receiver that we want to query.
 * \param[out] universe_id The retrieved universe.
 * \return #kEtcPalErrOk: Universe retrieved successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_get_universe(sacn_merge_receiver_t handle, uint16_t* universe_id)
{
  // TODO CHRISTIAN CLEANUP
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe_id);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the universe on which a sACN Merge Receiver is listening.
 *
 * An sACN merge receiver can only listen on one universe at a time. After this call completes, underlying updates will
 * generate new calls to SacnMergeReceiverMergedDataCallback(). If this call fails, the caller must call
 * sacn_merge_receiver_destroy for the merge receiver, because the merge receiver may be in an invalid state.
 *
 * \param[in] handle Handle to the merge receiver for which to change the universe.
 * \param[in] new_universe_id New universe number that this merge receiver should listen to.
 * \return #kEtcPalErrOk: Universe changed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified new universe.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_change_universe(sacn_merge_receiver_t handle, uint16_t new_universe_id)
{
  if (!UNIVERSE_ID_VALID(new_universe_id))
    return kEtcPalErrInvalid;

  // TODO CHRISTIAN CLEANUP
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Resets the underlying network sockets and packet receipt state for the sACN Merge Receiver..
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes, underlying updates will generate new calls to SacnMergeReceiverMergedDataCallback(). If
 * this call fails, the caller must call sacn_merge_receiver_destroy for the merge receiver, because the receiver may be
 * in an invalid state.
 *
 * \param[in] handle Handle to the merge receiver for which to reset the networking.
 * \param[in] (optional) array of network interfaces on which to listen to the specified universe. If NULL,
 *  all available network interfaces will be used.
 * \param[in] Number of elements in the netints array.
 * \return #kEtcPalErrOk: Network reset successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_reset_networking(sacn_merge_receiver_t handle, const EtcPalMcastNetintId* netints,
                                                    size_t num_netints)
{
  // TODO CHRISTIAN CLEANUP
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Returns the source id for that source cid.
 *
 * \param[in] handle The handle to the merge receiver.
 * \param[in] source_cid The UUID of the source CID.
 * \return The source ID, or #SACN_DMX_MERGER_SOURCE_INVALID.
 */
sacn_source_id_t sacn_merge_receiver_get_source_id(sacn_merge_receiver_t handle, const EtcPalUuid* source_cid)
{
  // TODO CHRISTIAN CLEANUP
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(source_cid);
  return SACN_DMX_MERGER_SOURCE_INVALID;
}

/*!
 * \brief fills in the source cid for that source id.
 *
 * \param[in] handle The handle to the merge receiver.
 * \param[in] source_id The ID of the source.
 * \param[out] source_cid The UUID of the source CID.
 * \return #kEtcPalErrOk: Lookup was successful.
 * \return #kEtcPalErrNotFound: handle does not correspond to a valid merge receiver, or source_id  does not correspond
 * to a valid source.
 */
etcpal_error_t sacn_merge_receiver_get_source_cid(sacn_merge_receiver_t handle, sacn_source_id_t source_id,
                                                  EtcPalUuid* source_cid)
{
  // TODO CHRISTIAN CLEANUP
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(source_id);
  ETCPAL_UNUSED_ARG(source_cid);
  return kEtcPalErrNotImpl;
}
