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

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "sacn/dmx_merger.h"
#include "sacn/receiver.h"
#include "sacn/private/mem.h"
#include "sacn/private/util.h"
#include "sacn/private/merge_receiver.h"

/***************************** Private constants *****************************/

/****************************** Private macros *******************************/

/**************************** Private variables ******************************/

/*********************** Private function prototypes *************************/

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Merge Receiver module. Internal function called from sacn_init(). */
etcpal_error_t sacn_merge_receiver_init(void)
{
  return kEtcPalErrOk;  // Nothing to do here.
}

/* Deinitialize the sACN Merge Receiver module. Internal function called from sacn_deinit(). */
void sacn_merge_receiver_deinit(void)
{
  // Nothing to do here.
}

/**
 * @brief Initialize an sACN Merge Receiver Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_merge_receiver_config_init(SacnMergeReceiverConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(SacnMergeReceiverConfig));
    config->use_pap = true;
    config->ip_supported = kSacnIpV4AndIpV6;
  }
}

/**
 * @brief Create a new sACN Merge Receiver to listen and merge sACN data on a universe.
 *
 * An sACN merge receiver can listen on one universe at a time, and each universe can only be listened to
 * by one merge receiver at at time.
 *
 * Note that a merge receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces listed in the passed in.  This will only return #kEtcPalErrNoNetints
 * if none of the interfaces work.
 *
 * @param[in] config Configuration parameters for the sACN Merge Receiver to be created.
 * @param[out] handle Filled in on success with a handle to the sACN Merge Receiver.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Merge Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this merge receiver, or maximum merge receivers reached.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_create(const SacnMergeReceiverConfig* config, sacn_merge_receiver_t* handle,
                                          SacnMcastInterface* netints, size_t num_netints)
{
  etcpal_error_t result = kEtcPalErrOk;

  sacn_dmx_merger_t merger_handle;
  sacn_receiver_t receiver_handle;

  if (!config || !handle)
    result = kEtcPalErrInvalid;

  if (result == kEtcPalErrOk)
  {

  }

  return result;
}

/**
 * @brief Destroy a sACN Merge Receiver instance.
 *
 * @param[in] handle Handle to the merge receiver to destroy.
 * @return #kEtcPalErrOk: Merge receiver destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_destroy(sacn_merge_receiver_t handle)
{
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Get the universe on which a sACN Merge Receiver is currently listening.
 *
 * @param[in] handle Handle to the merge receiver that we want to query.
 * @param[out] universe_id The retrieved universe.
 * @return #kEtcPalErrOk: Universe retrieved successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_get_universe(sacn_merge_receiver_t handle, uint16_t* universe_id)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe_id);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Change the universe on which a sACN Merge Receiver is listening.
 *
 * An sACN merge receiver can only listen on one universe at a time. After this call completes, a new sampling period
 * will occur, and then underlying updates will generate new calls to SacnMergeReceiverMergedDataCallback(). If this
 * call fails, the caller must call sacn_merge_receiver_destroy for the merge receiver, because the merge receiver may
 * be in an invalid state.
 *
 * @param[in] handle Handle to the merge receiver for which to change the universe.
 * @param[in] new_universe_id New universe number that this merge receiver should listen to.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified new universe.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_change_universe(sacn_merge_receiver_t handle, uint16_t new_universe_id)
{
  if (!UNIVERSE_ID_VALID(new_universe_id))
    return kEtcPalErrInvalid;

  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN Merge Receiver.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes, a new sampling period occurs, and then underlying updates will generate new calls to
 * SacnMergeReceiverMergedDataCallback(). If this call fails, the caller must call sacn_merge_receiver_destroy for the
 * merge receiver, because the merge receiver may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the merge receiver for which to reset the networking.
 * @param[in,out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Network reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_reset_networking(sacn_merge_receiver_t handle, SacnMcastInterface* netints,
                                                    size_t num_netints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Obtain the statuses of a merge receiver's network interfaces.
 *
 * @param[in] handle Handle to the merge receiver for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the merge receiver. If this is greater than netints_size, then
 * only netints_size addresses were written to the netints array. If the merge receiver was not found, 0 is returned.
 */
size_t sacn_merge_receiver_get_network_interfaces(sacn_merge_receiver_t handle, SacnMcastInterface* netints,
                                                  size_t netints_size)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(netints_size);

  return 0;  // TODO
}

/**
 * @brief Converts a source CID to the corresponding source ID, or #SACN_DMX_MERGER_SOURCE_INVALID if not found.
 *
 * This is a simple conversion from a source CID to it's corresponding source ID. A source ID will be returned only if
 * it is a source that has been discovered by the merge receiver.
 *
 * @param[in] handle The handle to the merge receiver.
 * @param[in] source_cid The UUID of the source CID.
 * @return The source ID, or #SACN_DMX_MERGER_SOURCE_INVALID if not found.
 */
sacn_source_id_t sacn_merge_receiver_get_source_id(sacn_merge_receiver_t handle, const EtcPalUuid* source_cid)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(source_cid);
  return SACN_DMX_MERGER_SOURCE_INVALID;
}

/**
 * @brief Converts a source ID to the corresponding source CID.
 *
 * @param[in] handle The handle to the merge receiver.
 * @param[in] source_id The ID of the source.
 * @param[out] source_cid The UUID of the source CID.
 * @return #kEtcPalErrOk: Lookup was successful.
 * @return #kEtcPalErrNotFound: handle does not correspond to a valid merge receiver, or source_id  does not correspond
 * to a valid source.
 */
etcpal_error_t sacn_merge_receiver_get_source_cid(sacn_merge_receiver_t handle, sacn_source_id_t source_id,
                                                  EtcPalUuid* source_cid)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(source_id);
  ETCPAL_UNUSED_ARG(source_cid);
  return kEtcPalErrNotImpl;
}
