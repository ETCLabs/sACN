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
#include "sacn/private/dmx_merger.h"
#include "sacn/private/receiver.h"
#include "sacn/private/receiver_state.h"
#include "sacn/private/mem.h"
#include "sacn/private/util.h"
#include "sacn/private/merge_receiver.h"

#if !SACN_DYNAMIC_MEM && (SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER != SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE)
#error \
    "SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER is invalid! The Merge Receiver API requires that it be equal to SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE."
#endif

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

  if (sacn_lock())
  {
    sacn_receiver_t receiver_handle = SACN_RECEIVER_INVALID;
    if (result == kEtcPalErrOk)
    {
      SacnReceiverConfig receiver_config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
      receiver_config.universe_id = config->universe_id;
      receiver_config.callbacks.universe_data = merge_receiver_universe_data;
      receiver_config.callbacks.sources_lost = merge_receiver_sources_lost;
      receiver_config.callbacks.sampling_period_started = merge_receiver_sampling_started;
      receiver_config.callbacks.sampling_period_ended = merge_receiver_sampling_ended;
      receiver_config.callbacks.source_pap_lost = merge_receiver_pap_lost;
      receiver_config.callbacks.source_limit_exceeded = merge_receiver_source_limit_exceeded;
      receiver_config.source_count_max = config->source_count_max;
      receiver_config.flags = SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA;
      receiver_config.ip_supported = config->ip_supported;
      result = create_sacn_receiver(&receiver_config, &receiver_handle, netints, num_netints);
    }

    // Since a merge receiver is a specialized receiver, and the handles are integers, just reuse the same value.
    sacn_merge_receiver_t merge_receiver_handle = (sacn_merge_receiver_t)receiver_handle;

    SacnMergeReceiver* merge_receiver = NULL;
    if (result == kEtcPalErrOk)
      result = add_sacn_merge_receiver(merge_receiver_handle, config, &merge_receiver);

    sacn_dmx_merger_t merger_handle = SACN_DMX_MERGER_INVALID;
    if (result == kEtcPalErrOk)
    {
      *handle = merge_receiver_handle;

      SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
      merger_config.slots = merge_receiver->slots;
      merger_config.slot_owners = merge_receiver->slot_owners;
      merger_config.source_count_max = config->source_count_max;
      result = create_sacn_dmx_merger(&merger_config, &merger_handle);
    }

    if (result == kEtcPalErrOk)
      merge_receiver->merger_handle = merger_handle;

    if (result != kEtcPalErrOk)
    {
      if (receiver_handle != SACN_RECEIVER_INVALID)
        destroy_sacn_receiver(receiver_handle);
      if (merger_handle != SACN_DMX_MERGER_INVALID)
        destroy_sacn_dmx_merger(merger_handle);

      size_t index = 0;
      if (lookup_merge_receiver((sacn_merge_receiver_t)receiver_handle, &merge_receiver, &index) == kEtcPalErrOk)
        remove_sacn_merge_receiver(index);
    }

    sacn_unlock();
  }
  else
  {
    result = kEtcPalErrSys;
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

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    size_t index = 0;
    if (result == kEtcPalErrOk)
      result = lookup_merge_receiver(handle, &merge_receiver, &index);

    if (result == kEtcPalErrOk)
    {
      destroy_sacn_receiver((sacn_receiver_t)handle);
      destroy_sacn_dmx_merger(merge_receiver->merger_handle);
      remove_sacn_merge_receiver(index);
    }

    sacn_unlock();
  }
  else
  {
    result = kEtcPalErrSys;
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
  // Use the public receiver API function directly, which takes the lock.
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

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_merge_receiver(handle, &merge_receiver, NULL);

    if (result == kEtcPalErrOk)
      result = change_sacn_receiver_universe((sacn_receiver_t)handle, new_universe_id);

    if (result == kEtcPalErrOk)
    {
      EtcPalRbIter iter;
      etcpal_rbiter_init(&iter);
      for (SacnMergeReceiverSource* src = etcpal_rbiter_first(&iter, &merge_receiver->sources);
           src && (result == kEtcPalErrOk); src = etcpal_rbiter_next(&iter))
      {
        result = remove_sacn_dmx_merger_source(merge_receiver->merger_handle, (sacn_dmx_merger_source_t)src->handle);
      }
    }

    if (result == kEtcPalErrOk)
      clear_sacn_merge_receiver_sources(merge_receiver);

    sacn_unlock();
  }
  else
  {
    result = kEtcPalErrSys;
  }

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
  // Use the public receiver API function directly, which takes the lock.
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

  if (!sacn_initialized())
    result = kEtcPalErrNotInit;
  else if (!netint_lists || (num_netint_lists == 0))
    result = kEtcPalErrInvalid;
#if !SACN_DYNAMIC_MEM
  else if (num_netint_lists > SACN_RECEIVER_MAX_UNIVERSES)
    result = kEtcPalErrInvalid;
#endif

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
#endif

  if (result == kEtcPalErrOk)
  {
    for (size_t i = 0; i < num_netint_lists; ++i)
    {
      receiver_netint_lists[i].handle = (sacn_receiver_t)netint_lists[i].handle;
      receiver_netint_lists[i].netints = netint_lists[i].netints;
      receiver_netint_lists[i].num_netints = netint_lists[i].num_netints;
    }

    // Now use the public receiver API function directly, which takes the lock.
    result = sacn_receiver_reset_networking_per_receiver(receiver_netint_lists, num_netint_lists);
  }

#if SACN_DYNAMIC_MEM
  if (receiver_netint_lists)
    free(receiver_netint_lists);
#endif

  return result;
}

/**
 * @brief Obtain a list of a merge receiver's network interfaces.
 *
 * @param[in] handle Handle to the merge receiver for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the merge receiver. If this is greater than netints_size, then
 * only netints_size entries were written to the netints array. If the merge receiver was not found, 0 is returned.
 */
size_t sacn_merge_receiver_get_network_interfaces(sacn_merge_receiver_t handle, EtcPalMcastNetintId* netints,
                                                  size_t netints_size)
{
  // Use the public receiver API function directly, which takes the lock.
  return sacn_receiver_get_network_interfaces((sacn_receiver_t)handle, netints, netints_size);
}

/**************************************************************************************************
 * Receiver callback implementations
 *************************************************************************************************/

void merge_receiver_universe_data(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                  const SacnHeaderData* header, const uint8_t* pdata, bool is_sampling, void* context)
{
  ETCPAL_UNUSED_ARG(is_sampling);
  ETCPAL_UNUSED_ARG(context);

  sacn_remote_source_t source_handle = header->source_handle;

  // Reuse source_handle for the DMX merger's source IDs, so it can be used in the merged_data callback.
  sacn_dmx_merger_source_t merger_source_handle = (sacn_dmx_merger_source_t)source_handle;

  MergeReceiverMergedDataNotification merged_data_notification = MERGE_RECV_MERGED_DATA_DEFAULT_INIT;
  MergeReceiverNonDmxNotification non_dmx_notification = MERGE_RECV_NON_DMX_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if (lookup_merge_receiver((sacn_merge_receiver_t)receiver_handle, &merge_receiver, NULL) == kEtcPalErrOk)
    {
      SacnMergeReceiverSource* source = NULL;
      if (lookup_merge_receiver_source(merge_receiver, source_handle, &source) == kEtcPalErrOk)
      {
        // The source is pending until the first 0x00 packet is received. After the sampling period, this indicates that
        // 0xDD must have either already been notified or timed out.
        if (source->pending && (header->start_code == 0x00))
        {
          source->pending = false;
          --merge_receiver->num_pending_sources;
        }
      }
      else
      {
        add_sacn_dmx_merger_source_with_handle(merge_receiver->merger_handle, merger_source_handle);

        add_sacn_merge_receiver_source(merge_receiver, source_handle,
                                       (merge_receiver->use_pap && (header->start_code == 0xDD)));
      }

      bool new_merge_occurred = false;
      if ((header->slot_count > 0) && (header->slot_count <= DMX_ADDRESS_COUNT))
      {
        if (header->start_code == 0x00)
        {
          update_sacn_dmx_merger_levels(merge_receiver->merger_handle, merger_source_handle, pdata, header->slot_count);
          update_sacn_dmx_merger_universe_priority(merge_receiver->merger_handle, merger_source_handle,
                                                   header->priority);
          new_merge_occurred = true;
        }
        else if ((header->start_code == 0xDD) && merge_receiver->use_pap)
        {
          update_sacn_dmx_merger_paps(merge_receiver->merger_handle, merger_source_handle, pdata, header->slot_count);
          new_merge_occurred = true;
        }
      }

      // Notify if needed.
      if (new_merge_occurred && !merge_receiver->sampling && (merge_receiver->num_pending_sources == 0))
      {
        merged_data_notification.callback = merge_receiver->callbacks.universe_data;
        merged_data_notification.handle = (sacn_merge_receiver_t)receiver_handle;
        merged_data_notification.universe = header->universe_id;
        memcpy(merged_data_notification.slots, merge_receiver->slots, DMX_ADDRESS_COUNT);
        memcpy(merged_data_notification.slot_owners, merge_receiver->slot_owners,
               DMX_ADDRESS_COUNT * sizeof(sacn_remote_source_t));  // Cast back to sacn_remote_source_t
        merged_data_notification.context = merge_receiver->callbacks.callback_context;
      }

      if ((header->start_code != 0x00) && (header->start_code != 0xDD))
      {
        non_dmx_notification.callback = merge_receiver->callbacks.universe_non_dmx;
        non_dmx_notification.receiver_handle = (sacn_merge_receiver_t)receiver_handle;
        non_dmx_notification.universe = header->universe_id;
        non_dmx_notification.source_addr = source_addr;
        non_dmx_notification.header = header;
        non_dmx_notification.pdata = pdata;
        non_dmx_notification.context = merge_receiver->callbacks.callback_context;
      }
    }

    sacn_unlock();
  }

  if (merged_data_notification.callback)
  {
    merged_data_notification.callback(merged_data_notification.handle, merged_data_notification.universe,
                                      merged_data_notification.slots, merged_data_notification.slot_owners,
                                      merged_data_notification.context);
  }

  if (non_dmx_notification.callback)
  {
    non_dmx_notification.callback(non_dmx_notification.receiver_handle, non_dmx_notification.universe,
                                  non_dmx_notification.source_addr, non_dmx_notification.header,
                                  non_dmx_notification.pdata, non_dmx_notification.context);
  }
}

void merge_receiver_sources_lost(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                                 size_t num_lost_sources, void* context)
{
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(context);

  MergeReceiverMergedDataNotification merged_data_notification = MERGE_RECV_MERGED_DATA_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if (lookup_merge_receiver((sacn_merge_receiver_t)handle, &merge_receiver, NULL) == kEtcPalErrOk)
    {
      for (size_t i = 0; i < num_lost_sources; ++i)
      {
        remove_sacn_merge_receiver_source(merge_receiver, lost_sources[i].handle);

        // The receiver handle is interchangable with the DMX Merger source IDs, so use it here via cast.
        remove_sacn_dmx_merger_source(merge_receiver->merger_handle, (sacn_dmx_merger_source_t)lost_sources[i].handle);
      }

      if (!merge_receiver->sampling && (merge_receiver->num_pending_sources == 0))
      {
        merged_data_notification.callback = merge_receiver->callbacks.universe_data;
        merged_data_notification.handle = (sacn_merge_receiver_t)handle;
        merged_data_notification.universe = universe;
        memcpy(merged_data_notification.slots, merge_receiver->slots, DMX_ADDRESS_COUNT);
        memcpy(merged_data_notification.slot_owners, merge_receiver->slot_owners,
               DMX_ADDRESS_COUNT * sizeof(sacn_remote_source_t));  // Cast back to sacn_remote_source_t        
        merged_data_notification.context = merge_receiver->callbacks.callback_context;
      }
    }

    sacn_unlock();
  }

  if (merged_data_notification.callback)
  {
    merged_data_notification.callback(merged_data_notification.handle, merged_data_notification.universe,
                                      merged_data_notification.slots, merged_data_notification.slot_owners,
                                      merged_data_notification.context);
  }
}

void merge_receiver_sampling_started(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(context);

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if (lookup_merge_receiver((sacn_merge_receiver_t)handle, &merge_receiver, NULL) == kEtcPalErrOk)
    {
      merge_receiver->sampling = true;
    }

    sacn_unlock();
  }
}

void merge_receiver_sampling_ended(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  MergeReceiverMergedDataNotification merged_data_notification = MERGE_RECV_MERGED_DATA_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if (lookup_merge_receiver((sacn_merge_receiver_t)handle, &merge_receiver, NULL) == kEtcPalErrOk)
    {
      merge_receiver->sampling = false;

      if ((etcpal_rbtree_size(&merge_receiver->sources) > 0) && (merge_receiver->num_pending_sources == 0))
      {
        merged_data_notification.callback = merge_receiver->callbacks.universe_data;
        merged_data_notification.handle = (sacn_merge_receiver_t)handle;
        merged_data_notification.universe = universe;
        memcpy(merged_data_notification.slots, merge_receiver->slots, DMX_ADDRESS_COUNT);
        memcpy(merged_data_notification.slot_owners, merge_receiver->slot_owners,
               DMX_ADDRESS_COUNT * sizeof(sacn_remote_source_t));  // Cast back to sacn_remote_source_t        
        merged_data_notification.context = merge_receiver->callbacks.callback_context;
      }
    }

    sacn_unlock();
  }

  if (merged_data_notification.callback)
  {
    merged_data_notification.callback(merged_data_notification.handle, merged_data_notification.universe,
                                      merged_data_notification.slots, merged_data_notification.slot_owners,
                                      merged_data_notification.context);
  }
}

void merge_receiver_pap_lost(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  MergeReceiverMergedDataNotification merged_data_notification = MERGE_RECV_MERGED_DATA_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if ((lookup_merge_receiver((sacn_merge_receiver_t)handle, &merge_receiver, NULL) == kEtcPalErrOk) &&
        merge_receiver->use_pap)
    {
      // The receiver handle is interchangable with the DMX Merger source IDs, so use it here via cast.
      remove_sacn_dmx_merger_paps(merge_receiver->merger_handle, (sacn_dmx_merger_source_t)source->handle);

      if (!merge_receiver->sampling && (merge_receiver->num_pending_sources == 0))
      {
        merged_data_notification.callback = merge_receiver->callbacks.universe_data;
        merged_data_notification.handle = (sacn_merge_receiver_t)handle;
        merged_data_notification.universe = universe;
        memcpy(merged_data_notification.slots, merge_receiver->slots, DMX_ADDRESS_COUNT);
        memcpy(merged_data_notification.slot_owners, merge_receiver->slot_owners,
               DMX_ADDRESS_COUNT * sizeof(sacn_remote_source_t));  // Cast back to sacn_remote_source_t        
        merged_data_notification.context = merge_receiver->callbacks.callback_context;
      }
    }

    sacn_unlock();
  }

  if (merged_data_notification.callback)
  {
    merged_data_notification.callback(merged_data_notification.handle, merged_data_notification.universe,
                                      merged_data_notification.slots, merged_data_notification.slot_owners,
                                      merged_data_notification.context);
  }
}

void merge_receiver_source_limit_exceeded(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(context);

  MergeReceiverSourceLimitExceededNotification limit_exceeded_notification =
      MERGE_RECV_SOURCE_LIMIT_EXCEEDED_DEFAULT_INIT;

  if (sacn_lock())
  {
    SacnMergeReceiver* merge_receiver = NULL;
    if (lookup_merge_receiver((sacn_merge_receiver_t)handle, &merge_receiver, NULL) == kEtcPalErrOk)
    {
      limit_exceeded_notification.callback = merge_receiver->callbacks.source_limit_exceeded;
      limit_exceeded_notification.handle = (sacn_merge_receiver_t)handle;
      limit_exceeded_notification.universe = universe;
      limit_exceeded_notification.context = merge_receiver->callbacks.callback_context;
    }

    sacn_unlock();
  }

  if (limit_exceeded_notification.callback)
  {
    limit_exceeded_notification.callback(limit_exceeded_notification.handle, limit_exceeded_notification.universe,
                                         limit_exceeded_notification.context);
  }
}
