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

/*********************** Receiver callback prototypes *************************/

static void universe_data(sacn_receiver_t handle, const EtcPalSockAddr* source_addr, const SacnHeaderData* header,
                          const uint8_t* pdata, bool is_sampling, void* context);
static void sources_lost(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                         size_t num_lost_sources, void* context);
static void sampling_started(sacn_receiver_t handle, uint16_t universe, void* context);
static void sampling_ended(sacn_receiver_t handle, uint16_t universe, void* context);
static void pap_lost(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, void* context);
static void source_limit_exceeded(sacn_receiver_t handle, uint16_t universe, void* context);

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

  if (!sacn_initialized())
    result = kEtcPalErrNotInit;
  else if (!config || !handle)
    result = kEtcPalErrInvalid;
  else if (!config->callbacks.universe_data || !config->callbacks.universe_non_dmx)
    result = kEtcPalErrInvalid;

  sacn_receiver_t receiver_handle = SACN_RECEIVER_INVALID;
  if (result == kEtcPalErrOk)
  {
    SacnReceiverConfig receiver_config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
    receiver_config.universe_id = config->universe_id;
    receiver_config.callbacks.universe_data = universe_data;
    receiver_config.callbacks.sources_lost = sources_lost;
    receiver_config.callbacks.sampling_period_started = sampling_started;
    receiver_config.callbacks.sampling_period_ended = sampling_ended;
    receiver_config.callbacks.source_pap_lost = pap_lost;
    receiver_config.callbacks.source_limit_exceeded = source_limit_exceeded;
    receiver_config.source_count_max = config->source_count_max;
    receiver_config.flags = SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA;
    receiver_config.ip_supported = config->ip_supported;

    result = sacn_receiver_create(&receiver_config, &receiver_handle, netints, num_netints);
  }

  SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Since a merge receiver is a specialized receiver, and the handles are integers, just reuse the same value.
      sacn_merge_receiver_t merge_receiver_handle = (sacn_merge_receiver_t)receiver_handle;

      SacnMergeReceiver* merge_receiver = NULL;
      result = add_sacn_merge_receiver(merge_receiver_handle, config, &merge_receiver);

      if (result == kEtcPalErrOk)
      {
        merger_config.slots = merge_receiver->slots;
        merger_config.slot_owners = merge_receiver->slot_owners;
        *handle = merge_receiver_handle;
      }

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  sacn_dmx_merger_t merger_handle = SACN_DMX_MERGER_INVALID;
  if (result == kEtcPalErrOk)
  {
    merger_config.source_count_max = config->source_count_max;
    result = sacn_dmx_merger_create(&merger_config, &merger_handle);
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      SacnMergeReceiver* merge_receiver = NULL;
      if (lookup_merge_receiver((sacn_merge_receiver_t)receiver_handle, &merge_receiver, NULL) == kEtcPalErrOk)
        merge_receiver->merger_handle = merger_handle;

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  if (result != kEtcPalErrOk)
  {
    if (receiver_handle != SACN_RECEIVER_INVALID)
      sacn_receiver_destroy(receiver_handle);
    if (merger_handle != SACN_DMX_MERGER_INVALID)
      sacn_dmx_merger_destroy(merger_handle);

    if (sacn_lock())
    {
      SacnMergeReceiver* tmp = NULL;
      size_t index = 0;
      if (lookup_merge_receiver((sacn_merge_receiver_t)receiver_handle, &tmp, &index) == kEtcPalErrOk)
        remove_sacn_merge_receiver(index);

      sacn_unlock();
    }
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
  etcpal_error_t result = kEtcPalErrOk;

  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  sacn_dmx_merger_t merger_handle = SACN_DMX_MERGER_INVALID;
  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      SacnMergeReceiver* merge_receiver = NULL;
      result = lookup_merge_receiver(handle, &merge_receiver, NULL);

      if ((result == kEtcPalErrOk) && (merge_receiver->merger_handle == SACN_DMX_MERGER_INVALID))
        result = kEtcPalErrNotFound;  // Merge receiver is still being created, so treat as not found.
      else
        merger_handle = merge_receiver->merger_handle;

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  if (result == kEtcPalErrOk)
  {
    etcpal_error_t receiver_result = sacn_receiver_destroy((sacn_receiver_t)handle);
    etcpal_error_t merger_result = sacn_dmx_merger_destroy(merger_handle);

    etcpal_error_t merge_receiver_result = kEtcPalErrOk;
    if (sacn_lock())
    {
      SacnMergeReceiver* tmp = NULL;
      size_t index = 0;
      merge_receiver_result = lookup_merge_receiver(handle, &tmp, &index);

      if (merge_receiver_result == kEtcPalErrOk)
        remove_sacn_merge_receiver(index);

      sacn_unlock();
    }
    else
    {
      merge_receiver_result = kEtcPalErrSys;
    }

    if (receiver_result != kEtcPalErrOk)
      result = receiver_result;
    else if (merger_result != kEtcPalErrOk)
      result = merger_result;
    else if (merge_receiver_result != kEtcPalErrOk)
      result = merge_receiver_result;
  }

  return result;
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
  return sacn_receiver_get_universe((sacn_receiver_t)handle, universe_id);
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
  etcpal_error_t result = kEtcPalErrOk;

  if (!sacn_initialized())
    result = kEtcPalErrNotInit;
  else if (!UNIVERSE_ID_VALID(new_universe_id))
    result = kEtcPalErrInvalid;

    // Determine what source IDs need removing
#if SACN_DYNAMIC_MEM
  sacn_source_id_t* sources_to_remove = NULL;
#else
  sacn_source_id_t sources_to_remove[SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE];
#endif
  size_t num_sources_to_remove = 0u;
  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      SacnMergeReceiver* merge_receiver = NULL;
      result = lookup_merge_receiver(handle, &merge_receiver, NULL);

      if (result == kEtcPalErrOk)
      {
        num_sources_to_remove = etcpal_rbtree_size(&merge_receiver->cids_from_ids);

        if (num_sources_to_remove > 0)
        {
#if SACN_DYNAMIC_MEM
          sources_to_remove = calloc(num_sources_to_remove, sizeof(sacn_source_id_t));
#endif
          size_t index = 0;
          EtcPalRbIter iter;
          etcpal_rbiter_init(&iter);
          for (sacn_source_id_t* id = etcpal_rbiter_first(&iter, &merge_receiver->cids_from_ids); id;
               id = etcpal_rbiter_next(&iter))
          {
            sources_to_remove[index] = *id;
            ++index;
          }
        }
      }

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  // Change receiver universe
  if (result == kEtcPalErrOk)
    result = sacn_receiver_change_universe((sacn_receiver_t)handle, new_universe_id);

  if (num_sources_to_remove > 0)
  {
    // Remove from merge receiver & get DMX merger handle
    sacn_dmx_merger_t merger_handle = SACN_DMX_MERGER_INVALID;
    if (result == kEtcPalErrOk)
    {
      if (sacn_lock())
      {
        SacnMergeReceiver* merge_receiver = NULL;
        result = lookup_merge_receiver(handle, &merge_receiver, NULL);

        if (result == kEtcPalErrOk)
        {
          merger_handle = merge_receiver->merger_handle;

          for (size_t i = 0; i < num_sources_to_remove; ++i)
            remove_sacn_merge_receiver_source(merge_receiver, sources_to_remove[i]);
        }

        sacn_unlock();
      }
      else
      {
        result = kEtcPalErrSys;
      }
    }

    // Remove from DMX merger
    for (size_t i = 0; (i < num_sources_to_remove) && (result == kEtcPalErrOk); ++i)
      result = sacn_dmx_merger_remove_source(merger_handle, sources_to_remove[i]);
  }

  // Clean up
#if SACN_DYNAMIC_MEM
  if (sources_to_remove)
    free(sources_to_remove);
#endif

  return result;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for all sACN merge receivers.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed, and wants
 * every merge receiver to use the same network interfaces.
 *
 * After this call completes, a new sampling period occurs, and then underlying updates will generate new calls to
 * SacnMergeReceiverMergedDataCallback(). If this call fails, the caller must call sacn_merge_receiver_destroy for each
 * merge receiver, because the merge receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in. This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints If non-NULL, this is the list of interfaces the application wants to use, and the status
 * codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_reset_networking(SacnMcastInterface* netints, size_t num_netints)
{
  return sacn_receiver_reset_networking(netints, num_netints);
}

/**
 * @brief Resets underlying network sockets and packet receipt state, determines network interfaces for each merge
 * receiver.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed, and wants to
 * determine what the new network interfaces should be for each merge receiver.
 *
 * After this call completes, a new sampling period occurs, and then underlying updates will generate new calls to
 * SacnMergeReceiverMergedDataCallback(). If this call fails, the caller must call sacn_merge_receiver_destroy for each
 * merge receiver, because the merge receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the network
 * interfaces passed in for each merge receiver. This will only return #kEtcPalErrNoNetints if none of the interfaces
 * work for a merge receiver.
 *
 * @param[in, out] netint_lists Lists of interfaces the application wants to use for each merge receiver. Must not be
 * NULL. Must include all merge receivers, and nothing more. The status codes are filled in whenever
 * SacnMergeReceiverNetintList::netints is non-NULL.
 * @param[in] num_netint_lists The size of netint_lists. Must not be 0.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided for a merge receiver were usable by the
 * library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_merge_receiver_reset_networking_per_receiver(const SacnMergeReceiverNetintList* netint_lists,
                                                                 size_t num_netint_lists)
{
  etcpal_error_t result = kEtcPalErrOk;

  if (!netint_lists || (num_netint_lists == 0))
    result = kEtcPalErrInvalid;

#if SACN_DYNAMIC_MEM
  SacnReceiverNetintList* receiver_netint_lists = NULL;
  if (result == kEtcPalErrOk)
  {
    receiver_netint_lists = calloc(num_netint_lists, sizeof(SacnReceiverNetintList));

    if (!receiver_netint_lists)
      result = kEtcPalErrNoMem;
  }
#else
  SacnReceiverNetintList receiver_netint_lists[SACN_RECEIVER_MAX_UNIVERSES];

  if (num_netint_lists > SACN_RECEIVER_MAX_UNIVERSES)
    result = kEtcPalErrInvalid;
#endif

  if (result == kEtcPalErrOk)
  {
    for (size_t i = 0; i < num_netint_lists; ++i)
    {
      receiver_netint_lists[i].handle = (sacn_receiver_t)netint_lists[i].handle;
      receiver_netint_lists[i].netints = netint_lists[i].netints;
      receiver_netint_lists[i].num_netints = netint_lists[i].num_netints;
    }

    result = sacn_receiver_reset_networking_per_receiver(receiver_netint_lists, num_netint_lists);
  }

#if SACN_DYNAMIC_MEM
  if (receiver_netint_lists)
    free(receiver_netint_lists);
#endif

  return result;
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

/**************************************************************************************************
 * Receiver callback implementations
 *************************************************************************************************/

void universe_data(sacn_receiver_t handle, const EtcPalSockAddr* source_addr, const SacnHeaderData* header,
                   const uint8_t* pdata, bool is_sampling, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(source_addr);
  ETCPAL_UNUSED_ARG(header);
  ETCPAL_UNUSED_ARG(pdata);
  ETCPAL_UNUSED_ARG(is_sampling);
  ETCPAL_UNUSED_ARG(context);
}

void sources_lost(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                  size_t num_lost_sources, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(lost_sources);
  ETCPAL_UNUSED_ARG(num_lost_sources);
  ETCPAL_UNUSED_ARG(context);
}

void sampling_started(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(context);
}

void sampling_ended(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(context);
}

void pap_lost(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(source);
  ETCPAL_UNUSED_ARG(context);
}

void source_limit_exceeded(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(context);
}
