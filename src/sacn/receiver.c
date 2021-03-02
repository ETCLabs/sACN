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

#include "sacn/receiver.h"

#include <stdint.h>
#include "etcpal/common.h"
#include "sacn/private/common.h"
#include "sacn/private/source_loss.h"
#include "sacn/private/mem.h"
#include "sacn/private/receiver.h"
#include "sacn/private/receiver_state.h"

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Receiver module. Internal function called from sacn_init(). */
etcpal_error_t sacn_receiver_init(void)
{
  return kEtcPalErrOk;  // Nothing to do here.
}

/* Deinitialize the sACN Receiver module. Internal function called from sacn_deinit(). */
void sacn_receiver_deinit(void)
{
  // Nothing to do here.
}

/**
 * @brief Initialize an sACN Receiver Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_receiver_config_init(SacnReceiverConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(SacnReceiverConfig));
    config->source_count_max = SACN_RECEIVER_INFINITE_SOURCES;
  }
}

/**
 * @brief Create a new sACN receiver to listen for sACN data on a universe.
 *
 * A sACN receiver can listen on one universe at a time, and each universe can only be listened to
 * by one receiver at at time.
 *
 * After this call completes successfully, the receiver is in a sampling period for the universe and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for the universe.
 *
 * Note that a receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] config Configuration parameters for the sACN receiver to be created.
 * @param[out] handle Filled in on success with a handle to the sACN receiver.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_create(const SacnReceiverConfig* config, sacn_receiver_t* handle,
                                    SacnMcastInterface* netints, size_t num_netints)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
  {
    res = kEtcPalErrNotInit;
  }
  else if (!config || !handle)
  {
    res = kEtcPalErrInvalid;
  }
  else if (!UNIVERSE_ID_VALID(config->universe_id) || !config->callbacks.universe_data ||
           !config->callbacks.sources_lost || !config->callbacks.sampling_period_ended)
  {
    res = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    SacnReceiver* receiver = NULL;
    if (res == kEtcPalErrOk)
      res = add_sacn_receiver(get_next_receiver_handle(), config, netints, num_netints, &receiver);

    if (res == kEtcPalErrOk)
    {
      begin_sampling_period(receiver);
      res = assign_receiver_to_thread(receiver);
    }

    if (res == kEtcPalErrOk)
    {
      *handle = receiver->keys.handle;
    }
    else
    {
      if (receiver)
      {
        remove_receiver_from_thread(receiver, kCloseSocketNow);
        remove_sacn_receiver(receiver);
      }
    }
    sacn_unlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Destroy a sACN receiver instance.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 *
 * @param[in] handle Handle to the receiver to destroy.
 * @return #kEtcPalErrOk: Receiver destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_destroy(sacn_receiver_t handle)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;

  if (sacn_lock())
  {
    SacnReceiver* receiver = NULL;
    if (res == kEtcPalErrOk)
      res = lookup_receiver(handle, &receiver);

    if (res == kEtcPalErrOk)
    {
      remove_receiver_from_thread(receiver, kQueueSocketForClose);
      remove_sacn_receiver(receiver);
    }

    sacn_unlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Get the universe on which a sACN receiver is currently listening.
 *
 * @param[in] handle Handle to the receiver that we want to query.
 * @param[out] universe_id The retrieved universe.
 * @return #kEtcPalErrOk: Universe retrieved successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_get_universe(sacn_receiver_t handle, uint16_t* universe_id)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;
  else if (universe_id == NULL)
    res = kEtcPalErrInvalid;

  if (sacn_lock())
  {
    SacnReceiver* receiver = NULL;
    if (res == kEtcPalErrOk)
      res = lookup_receiver(handle, &receiver);

    if (res == kEtcPalErrOk)
      *universe_id = receiver->keys.universe;

    sacn_unlock();
  }

  return res;
}

/**
 * @brief Change the universe on which an sACN receiver is listening.
 *
 * An sACN receiver can only listen on one universe at a time. After this call completes successfully, the receiver is
 * in a sampling period for the universe and will provide SamplingPeriodStarted() and SamplingPeriodEnded()
 * notifications, as well as UniverseData() notifications as packets are received for the universe. If this call fails,
 * the caller must call sacn_receiver_destroy for the receiver, because the receiver may be in an invalid state.
 *
 * @param[in] handle Handle to the receiver for which to change the universe.
 * @param[in] new_universe_id New universe number that this receiver should listen to.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified new universe.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_change_universe(sacn_receiver_t handle, uint16_t new_universe_id)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;
  else if (!UNIVERSE_ID_VALID(new_universe_id))
    res = kEtcPalErrInvalid;

  if (sacn_lock())
  {
    // First check to see if there is already a receiver listening on this universe.
    if (res == kEtcPalErrOk)
    {
      SacnReceiver* tmp = NULL;
      if (lookup_receiver_by_universe(new_universe_id, &tmp) == kEtcPalErrOk)
        res = kEtcPalErrExists;
    }

    // Find the receiver to change the universe for.
    SacnReceiver* receiver = NULL;
    if (res == kEtcPalErrOk)
      res = lookup_receiver(handle, &receiver);

    // Clear termination sets and sources since they only pertain to the old universe.
    if (res == kEtcPalErrOk)
      res = clear_term_sets_and_sources(receiver);

    // Update receiver key and position in receiver_state.receivers_by_universe.
    if (res == kEtcPalErrOk)
      res = update_receiver_universe(receiver, new_universe_id);

    // Update the receiver's socket and subscription.
    if (res == kEtcPalErrOk)
    {
      remove_receiver_sockets(receiver, kQueueSocketForClose);
      res = add_receiver_sockets(receiver);
    }

    // Begin the sampling period.
    if (res == kEtcPalErrOk)
      begin_sampling_period(receiver);

    sacn_unlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for all sACN receivers.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed, and wants
 * every receiver to use the same network interfaces.
 *
 * After this call completes successfully, every receiver is in a sampling period for their universes and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for their universes. If this call fails, the caller must call sacn_receiver_destroy for each receiver,
 * because the receivers may be in an invalid state.
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
etcpal_error_t sacn_receiver_reset_networking(SacnMcastInterface* netints, size_t num_netints)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;

  if (sacn_lock())
  {
    if (res == kEtcPalErrOk)
      res = sacn_sockets_reset_receiver();

    if (res == kEtcPalErrOk)
    {
      // All current sockets need to be removed before adding new ones.
      remove_all_receiver_sockets(kQueueSocketForClose);

      EtcPalRbIter iter;
      etcpal_rbiter_init(&iter);
      for (SacnReceiver* receiver = etcpal_rbiter_first(&iter, &receiver_state.receivers);
           (res == kEtcPalErrOk) && receiver; receiver = etcpal_rbiter_next(&iter))
      {
        res = sacn_initialize_receiver_netints(&receiver->netints, netints, num_netints);
        if (res == kEtcPalErrOk)
          res = add_receiver_sockets(receiver);
        if (res == kEtcPalErrOk)
          res = reset_receiver_state(receiver);
      }
    }

    sacn_unlock();
  }

  return res;
}

/**
 * @brief Resets underlying network sockets and packet receipt state, determines network interfaces for each receiver.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed, and wants to
 * determine what the new network interfaces should be for each receiver.
 *
 * After this call completes successfully, every receiver is in a sampling period for their universes and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for their universes. If this call fails, the caller must call sacn_receiver_destroy for each receiver,
 * because the receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the network
 * interfaces passed in for each receiver. This will only return #kEtcPalErrNoNetints if none of the interfaces work for
 * a receiver.
 *
 * @param[in, out] netint_lists Lists of interfaces the application wants to use for each receiver. Must not be NULL.
 * Must include all receivers, and nothing more. The status codes are filled in whenever SacnReceiverNetintList::netints
 * is non-NULL.
 * @param[in] num_netint_lists The size of netint_lists. Must not be 0.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided for a receiver were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_reset_networking_per_receiver(const SacnReceiverNetintList* netint_lists,
                                                           size_t num_netint_lists)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;

  if ((netint_lists == NULL) || (num_netint_lists == 0))
    res = kEtcPalErrInvalid;

  if (sacn_lock())
  {
    // Validate netint_lists. It must include all receivers and nothing more.
    size_t total_num_receivers = 0;
    EtcPalRbIter iter;
    etcpal_rbiter_init(&iter);
    for (SacnReceiver* receiver = etcpal_rbiter_first(&iter, &receiver_state.receivers);
         (res == kEtcPalErrOk) && receiver; receiver = etcpal_rbiter_next(&iter))
    {
      ++total_num_receivers;

      bool found = false;
      for (size_t i = 0; !found && (i < num_netint_lists); ++i)
        found = (receiver->keys.handle == netint_lists[i].handle);

      if (!found)
        res = kEtcPalErrInvalid;
    }

    if (res == kEtcPalErrOk)
    {
      if (num_netint_lists != total_num_receivers)
        res = kEtcPalErrInvalid;
    }

    if (res == kEtcPalErrOk)
      res = sacn_sockets_reset_receiver();

    if (res == kEtcPalErrOk)
    {
      // All current sockets need to be removed before adding new ones.
      remove_all_receiver_sockets(kQueueSocketForClose);

      // After the old sockets have been removed, initialize the new netints, sockets, and state.
      for (size_t i = 0; (res == kEtcPalErrOk) && (i < num_netint_lists); ++i)
      {
        SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers, &netint_lists[i].handle);
        res =
            sacn_initialize_receiver_netints(&receiver->netints, netint_lists[i].netints, netint_lists[i].num_netints);
        if (res == kEtcPalErrOk)
          res = add_receiver_sockets(receiver);
        if (res == kEtcPalErrOk)
          res = reset_receiver_state(receiver);
      }
    }

    sacn_unlock();
  }

  return res;
}

/**
 * @brief Obtain a list of a receiver's network interfaces.
 *
 * @param[in] handle Handle to the receiver for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the receiver. If this is greater than netints_size, then only
 * netints_size entries were written to the netints array. If the receiver was not found, 0 is returned.
 */
size_t sacn_receiver_get_network_interfaces(sacn_receiver_t handle, EtcPalMcastNetintId* netints, size_t netints_size)
{
  size_t total_num_network_interfaces = 0;

  if (sacn_lock())
  {
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers, &handle);
    if (receiver)
    {
      total_num_network_interfaces = receiver->netints.num_netints;

      for (size_t i = 0; netints && (i < netints_size) && (i < total_num_network_interfaces); ++i)
        netints[i] = receiver->netints.netints[i];
    }

    sacn_unlock();
  }

  return total_num_network_interfaces;
}

/**
 * @brief Set the expired notification wait time.
 *
 * The library will wait at least this long after a source loss condition has been encountered before
 * sending a @ref SacnSourcesLostCallback "sources_lost()" notification. However, the wait may be
 * longer due to the source loss algorithm (see @ref source_loss_behavior).
 *
 * @param[in] wait_ms Wait time in milliseconds.
 */
void sacn_receiver_set_expired_wait(uint32_t wait_ms)
{
  if (!sacn_initialized())
    return;

  if (sacn_lock())
  {
    receiver_state.expired_wait = wait_ms;
    sacn_unlock();
  }
}

/**
 * @brief Get the current value of the expired notification wait time.
 *
 * The library will wait at least this long after a source loss condition has been encountered before
 * sending a @ref SacnSourcesLostCallback "sources_lost()" notification. However, the wait may be
 * longer due to the source loss algorithm (see @ref source_loss_behavior).
 *
 * @return Wait time in milliseconds.
 */
uint32_t sacn_receiver_get_expired_wait()
{
  uint32_t res = SACN_DEFAULT_EXPIRED_WAIT_MS;

  if (!sacn_initialized())
    return res;

  if (sacn_lock())
  {
    res = receiver_state.expired_wait;
    sacn_unlock();
  }
  return res;
}
