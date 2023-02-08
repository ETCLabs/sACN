/******************************************************************************
 * Copyright 2022 ETC Inc.
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

#include "sacn/source.h"
#include "sacn/private/source.h"
#include "sacn/private/source_state.h"
#include "sacn/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"

#if SACN_SOURCE_ENABLED || DOXYGEN

/**************************** Private function declarations ******************************/

static size_t get_per_universe_netint_lists_index(sacn_source_t source, uint16_t universe,
                                                  const SacnSourceUniverseNetintList* per_universe_netint_lists,
                                                  size_t num_per_universe_netint_lists, bool* found);

/*************************** Function definitions ****************************/

/* Initialize the sACN Source module. Internal function called from sacn_init(). */
etcpal_error_t sacn_source_init(void)
{
  return kEtcPalErrOk;  // Nothing to do here.
}

void sacn_source_deinit(void)
{
  // Nothing to do here.
}

/**
 * @brief Initialize an sACN Source Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_config_init(SacnSourceConfig* config)
{
  if (config)
  {
    config->cid = kEtcPalNullUuid;
    config->name = NULL;
    config->universe_count_max = SACN_SOURCE_INFINITE_UNIVERSES;
    config->manually_process_source = false;
    config->ip_supported = kSacnIpV4AndIpV6;
    config->keep_alive_interval = SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT;
    config->pap_keep_alive_interval = SACN_SOURCE_PAP_KEEP_ALIVE_INTERVAL_DEFAULT;
  }
}

/**
 * @brief Initialize an sACN Source Universe Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_universe_config_init(SacnSourceUniverseConfig* config)
{
  if (config)
  {
    config->universe = 0;
    config->priority = 100;
    config->send_preview = false;
    config->send_unicast_only = false;
    config->unicast_destinations = NULL;
    config->num_unicast_destinations = 0;
    config->sync_universe = 0;
  }
}

/**
 * @brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source and begins sending universe discovery packets for it (which will list no
 * universes until start code data begins transmitting). No start code data is sent until sacn_source_add_universe() and
 * a variant of sacn_source_update_levels() is called.
 *
 * @param[in] config Configuration parameters for the sACN source to be created. If any of these parameters are invalid,
 * #kEtcPalErrInvalid will be returned. This includes if the source name's length (including the null terminator) is
 * beyond #SACN_SOURCE_NAME_MAX_LEN.
 * @param[out] handle Filled in on success with a handle to the sACN source.
 * @return #kEtcPalErrOk: Source successfully created.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate an additional source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Other codes translated from system error codes are possible.
 */
etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (!config || ETCPAL_UUID_IS_NULL(&config->cid) || !config->name ||
        (strlen(config->name) > (SACN_SOURCE_NAME_MAX_LEN - 1)) || (config->keep_alive_interval <= 0) ||
        (config->pap_keep_alive_interval <= 0) || !handle)
    {
      result = kEtcPalErrInvalid;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // If the Tick thread hasn't been started yet, start it if the config isn't manual.
      if (!config->manually_process_source)
        result = initialize_source_thread();

      // Initialize the source's state.
      SacnSource* source = NULL;
      if (result == kEtcPalErrOk)
        result = add_sacn_source(get_next_source_handle(), config, &source);

      // Initialize the handle on success.
      if (result == kEtcPalErrOk)
        *handle = source->handle;

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the packet for use in
 * displaying the identity of a source to a user." If its length (including the null terminator) is longer than
 * #SACN_SOURCE_NAME_MAX_LEN, then #kEtcPalErrInvalid will be returned.
 *
 * This function will update the packet buffers of all this source's universes with the new name. For each universe that
 * is transmitting NULL start code or PAP data, the logic that slows down packet transmission due to inactivity will be
 * reset.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] new_name New name to use for this source.
 * @return #kEtcPalErrOk: Name set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if ((handle == SACN_SOURCE_INVALID) || !new_name || (strlen(new_name) > (SACN_SOURCE_NAME_MAX_LEN - 1)))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Look up the source's state.
      SacnSource* source = NULL;
      result = lookup_source(handle, &source);

      if ((result == kEtcPalErrOk) && source && source->terminating)
        result = kEtcPalErrNotFound;

      // Set this source's name.
      if (result == kEtcPalErrOk)
        set_source_name(source, new_name);

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. This removes the source and queues the sending of termination packets to
 * all of the source's universes, which takes place either on the thread or on calls to sacn_source_process_manual().
 * The source will also stop transmitting sACN universe discovery packets.
 *
 * @param[in] handle Handle to the source to destroy. This handle will no longer be usable with the source API.
 */
void sacn_source_destroy(sacn_source_t handle)
{
  // Validate and lock.
  if (sacn_initialized() && (handle != SACN_SOURCE_INVALID) && sacn_lock())
  {
    // Try to find the source's state.
    SacnSource* source = NULL;
    lookup_source(handle, &source);

    // If the source was found, initiate termination.
    if (source && !source->terminating)
      set_source_terminating(source);

    sacn_unlock();
  }
}

/**
 * @brief Add a universe to an sACN source.
 *
 * Adds a universe to a source.
 * After this call completes, the applicaton must call a variant of sacn_source_update_levels() to mark it ready for
 * processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe Discovery packets
 * once a variant of sacn_source_update_levels() is called.
 *
 * Note that a universe is considered as successfully added if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the source to which to add a universe.
 * @param[in] config Configuration parameters for the universe to be added.
 * @param[in, out] netint_config Optional. If non-NULL, this is the list of interfaces the application wants to use, and
 * the status codes are filled in.  If NULL, all available interfaces are tried.
 * @return #kEtcPalErrOk: Universe successfully added.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: Universe given was already added to this source.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                        const SacnNetintConfig* netint_config)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if ((handle == SACN_SOURCE_INVALID) || !config || !UNIVERSE_ID_VALID(config->universe) ||
        (config->sync_universe && !UNIVERSE_ID_VALID(config->sync_universe)) ||
        ((config->num_unicast_destinations > 0) && !config->unicast_destinations))
    {
      result = kEtcPalErrInvalid;
    }
    else
    {
      for (size_t i = 0; i < config->num_unicast_destinations; ++i)
      {
        if (ETCPAL_IP_IS_INVALID(&config->unicast_destinations[i]))
          result = kEtcPalErrInvalid;
      }
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Look up the source's state.
      SacnSource* source = NULL;
      result = lookup_source(handle, &source);

      if ((result == kEtcPalErrOk) && source->terminating)
        result = kEtcPalErrNotFound;

      // Handle the existing universe if there is one.
      if (result == kEtcPalErrOk)
      {
        bool found = false;
        size_t index = get_source_universe_index(source, config->universe, &found);

        if (found)
        {
          SacnSourceUniverse* existing_universe = &source->universes[index];

          if (existing_universe->termination_state == kTerminatingAndRemoving)
            finish_source_universe_termination(source, index);  // Remove the old state before adding the new.
          else
            result = kEtcPalErrExists;
        }
      }

      // Initialize the new universe's state.
      SacnSourceUniverse* new_universe = NULL;
      if (result == kEtcPalErrOk)
        result = add_sacn_source_universe(source, config, netint_config, &new_universe);

      // Update the source's netint tracking.
      for (size_t i = 0; (result == kEtcPalErrOk) && (i < new_universe->netints.num_netints); ++i)
        result = add_sacn_source_netint(source, &new_universe->netints.netints[i]);

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Remove a universe from a source.
 *
 * This removes a universe and queues the sending of termination packets to the universe, which takes place either on
 * the thread or on calls to sacn_source_process_manual().
 *
 * The source will also stop including the universe in sACN universe discovery packets.
 *
 * @param[in] handle Handle to the source from which to remove the universe.
 * @param[in] universe Universe to remove. The source API functions will no longer recognize this universe for this
 * source unless the universe is re-added to the source.
 */
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe)
{
  if (sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_and_universe(handle, universe, &source_state, &universe_state);

    if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
      set_universe_terminating(universe_state, kTerminateAndRemove);

    sacn_unlock();
  }
}

/**
 * @brief Obtain a list of a source's universes (sorted lowest to highest).
 *
 * @param[in] handle Handle to the source for which to obtain the list of universes.
 * @param[out] universes A pointer to an application-owned array where the universe list will be written.
 * @param[in] universes_size The size of the provided universes array.
 * @return The total number of the source's universes. If this is greater than universes_size, then only universes_size
 * universes were written to the universes array. If the source was not found, 0 is returned.
 */
size_t sacn_source_get_universes(sacn_source_t handle, uint16_t* universes, size_t universes_size)
{
  size_t total_num_universes = 0;

  if (sacn_lock())
  {
    // Look up source state
    SacnSource* source = NULL;
    if ((lookup_source(handle, &source) == kEtcPalErrOk) && source && !source->terminating)
      total_num_universes = get_source_universes(source, universes, universes_size);

    sacn_unlock();
  }

  return total_num_universes;
}

/**
 * @brief Add a unicast destination for a source's universe.
 *
 * This will reset transmission suppression and include the new unicast destination in transmissions for the universe.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL.
 * @return #kEtcPalErrOk: Address added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrExists: The unicast destination was already added to this universe on this source.
 * @return #kEtcPalErrNoMem: No room to allocate additional destination address.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if ((handle == SACN_SOURCE_INVALID) || !UNIVERSE_ID_VALID(universe) || !dest || ETCPAL_IP_IS_INVALID(dest))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Look up the state
      SacnSource* source_state = NULL;
      SacnSourceUniverse* universe_state = NULL;
      result = lookup_source_and_universe(handle, universe, &source_state, &universe_state);

      if ((result == kEtcPalErrOk) && (universe_state->termination_state == kTerminatingAndRemoving))
        result = kEtcPalErrNotFound;

      // Handle the existing unicast destination if there is one.
      if (result == kEtcPalErrOk)
      {
        bool found = false;
        size_t index = get_unicast_dest_index(universe_state, dest, &found);

        if (found)
        {
          SacnUnicastDestination* existing_unicast_dest = &universe_state->unicast_dests[index];

          if (existing_unicast_dest->termination_state == kTerminatingAndRemoving)
            finish_unicast_dest_termination(universe_state, index);  // Remove the old state before adding the new.
          else
            result = kEtcPalErrExists;
        }
      }

      // Add unicast destination
      SacnUnicastDestination* new_unicast_dest = NULL;
      if (result == kEtcPalErrOk)
        result = add_sacn_unicast_dest(universe_state, dest, &new_unicast_dest);

      // Initialize & reset transmission suppression
      if (result == kEtcPalErrOk)
        reset_transmission_suppression(source_state, universe_state, kResetLevelAndPap);

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Remove a unicast destination on a source's universe.
 *
 * This removes a unicast destination address and queues the sending of termination packets to the address, which takes
 * place either on the thread or on calls to sacn_source_process_manual().
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP to remove.  May not be NULL, and must match the address passed to
 * sacn_source_add_unicast_destination().
 */
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  // Validate & lock
  if (dest && sacn_lock())
  {
    // Look up unicast destination
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_and_universe(handle, universe, &source_state, &universe_state);

    if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
    {
      SacnUnicastDestination* unicast_dest = NULL;
      lookup_unicast_dest(universe_state, dest, &unicast_dest);

      // Initiate termination
      if (unicast_dest && (unicast_dest->termination_state != kTerminatingAndRemoving))
        set_unicast_dest_terminating(unicast_dest, kTerminateAndRemove);
    }

    sacn_unlock();
  }
}

/**
 * @brief Obtain a list of a universe's unicast destinations.
 *
 * @param[in] handle Handle to the source of the universe in question.
 * @param[in] universe The universe for which to obtain the list of unicast destinations.
 * @param[out] destinations A pointer to an application-owned array where the unicast destination list will be written.
 * @param[in] destinations_size The size of the provided destinations array.
 * @return The total number of unicast destinations for the given universe. If this is greater than destinations_size,
 * then only destinations_size addresses were written to the destinations array. If the source was not found, 0 is
 * returned.
 */
size_t sacn_source_get_unicast_destinations(sacn_source_t handle, uint16_t universe, EtcPalIpAddr* destinations,
                                            size_t destinations_size)
{
  size_t total_num_dests = 0;

  if (sacn_lock())
  {
    // Look up universe state
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    if (lookup_source_and_universe(handle, universe, &source_state, &universe_state) == kEtcPalErrOk)
    {
      if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
        total_num_dests = get_source_unicast_dests(universe_state, destinations, destinations_size);
    }

    sacn_unlock();
  }

  return total_num_dests;
}

/**
 * @brief Change the priority of a universe on a sACN source.
 *
 * This function will update the packet buffers with the new priority. If this universe is transmitting NULL start code
 * or PAP data, the logic that slows down packet transmission due to inactivity will be reset.
 *
 * @param[in] handle Handle to the source for which to set the priority.
 * @param[in] universe Universe to change.
 * @param[in] new_priority New priority of the data sent from this source. Valid range is 0 to 200,
 *                         inclusive.
 * @return #kEtcPalErrOk: Priority set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint16_t universe, uint8_t new_priority)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if ((handle == SACN_SOURCE_INVALID) || !UNIVERSE_ID_VALID(universe) || (new_priority > 200))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Look up the source and universe state.
      SacnSource* source_state = NULL;
      SacnSourceUniverse* universe_state = NULL;
      result = lookup_source_and_universe(handle, universe, &source_state, &universe_state);

      if ((result == kEtcPalErrOk) && universe_state && (universe_state->termination_state == kTerminatingAndRemoving))
        result = kEtcPalErrNotFound;

      // Set the priority.
      if (result == kEtcPalErrOk)
        set_universe_priority(source_state, universe_state, new_priority);

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Change the send_preview option on a universe of a sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
 *
 * This function will update the packet buffers with the new option. If this universe is transmitting NULL start code
 * or PAP data, the logic that slows down packet transmission due to inactivity will be reset.
 *
 * @param[in] handle Handle to the source for which to set the Preview_Data option.
 * @param[in] universe The universe to change.
 * @param[in] new_preview_flag The new send_preview option.
 * @return #kEtcPalErrOk: send_preview option set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, uint16_t universe, bool new_preview_flag)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if ((handle == SACN_SOURCE_INVALID) || !UNIVERSE_ID_VALID(universe))
      result = kEtcPalErrInvalid;
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Look up the source and universe state.
      SacnSource* source_state = NULL;
      SacnSourceUniverse* universe_state = NULL;
      result = lookup_source_and_universe(handle, universe, &source_state, &universe_state);

      if ((result == kEtcPalErrOk) && universe_state && (universe_state->termination_state == kTerminatingAndRemoving))
        result = kEtcPalErrNotFound;

      // Set the preview flag.
      if (result == kEtcPalErrOk)
        set_preview_flag(source_state, universe_state, new_preview_flag);

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Changes the synchronization universe for a universe of a sACN source.
 *
 * This will change the synchronization universe used by a sACN universe on the source.
 * If this value is 0, synchronization is turned off for that universe.
 *
 * This function will update the packet buffers with the new sync universe. If this universe is transmitting NULL start
 * code or PAP data, the logic that slows down packet transmission due to inactivity will be reset.
 *
 * @todo At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe The universe to change.
 * @param[in] new_sync_universe The new synchronization universe to set.
 * @return #kEtcPalErrOk: sync_universe set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_synchronization_universe(sacn_source_t handle, uint16_t universe,
                                                           uint16_t new_sync_universe)
{
  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_sync_universe);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Immediately sends the provided sACN start code & data.
 *
 * Immediately sends a sACN packet with the provided start code and data.
 * This function is intended for sACN packets that have a startcode other than 0 or 0xdd, since those
 * start codes are taken care of by either the thread or sacn_source_process_manual().
 *
 * @param[in] handle Handle to the source.
 * @param[in] universe Universe to send on.
 * @param[in] start_code The start code to send.
 * @param[in] buffer The buffer to send.  Must not be NULL.
 * @param[in] buflen The size of buffer.
 * @return #kEtcPalErrOk: Message successfully sent.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return The last error returned by etcpal_sendto() if all sends failed.
 */
etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen)
{
  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if ((handle == SACN_SOURCE_INVALID) || !UNIVERSE_ID_VALID(universe) || (buflen > DMX_ADDRESS_COUNT) || !buffer ||
        (buflen == 0))
    {
      result = kEtcPalErrInvalid;
    }
  }

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Look up state
      SacnSource* source_state = NULL;
      SacnSourceUniverse* universe_state = NULL;
      result = lookup_source_and_universe(handle, universe, &source_state, &universe_state);

      if ((result == kEtcPalErrOk) && universe_state && (universe_state->termination_state == kTerminatingAndRemoving))
        result = kEtcPalErrNotFound;

      if (result == kEtcPalErrOk)
      {
        // Initialize send buffer
        uint8_t send_buf[SACN_DATA_PACKET_MTU];
        init_sacn_data_send_buf(send_buf, start_code, &source_state->cid, source_state->name, universe_state->priority,
                                universe_state->universe_id, universe_state->sync_universe,
                                universe_state->send_preview);
        pack_sequence_number(send_buf, universe_state->next_seq_num);
        update_send_buf_data(send_buf, buffer, (uint16_t)buflen, kDisableForceSync);

        // Send on the network
        send_universe_multicast(source_state, universe_state, send_buf);
        send_universe_unicast(source_state, universe_state, send_buf);

        if (!universe_state->anything_sent_this_tick)
          result = universe_state->last_send_error;

        increment_sequence_number(universe_state);
      }

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Indicate that a new synchronization packet should be sent on the given synchronization universe.
 *
 * This will cause the source to transmit a synchronization packet on the given synchronization universe.
 *
 * @todo At this time, synchronization is not supported by this library, so this function is not implemented.
 *
 * @param[in] handle Handle to the source.
 * @param[in] sync_universe The synchronization universe to send on.
 * @return #kEtcPalErrOk: Message successfully sent.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_send_synchronization(sacn_source_t handle, uint16_t sync_universe)
{
  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(sync_universe);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Copies the universe's DMX levels into the packet to be sent on the next threaded or manual update.
 *
 * This function will update the outgoing packet data, and reset the logic that slows down packet transmission due to
 * inactivity.
 *
 * When you don't have per-address priority changes to make, use this function. Otherwise, use
 * sacn_source_update_levels_and_pap().
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_levels A buffer of DMX levels to copy from. If this pointer is NULL, the source will terminate DMX
 * transmission without removing the universe.
 * @param[in] new_levels_size Size of new_levels. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_levels(sacn_source_t handle, uint16_t universe, const uint8_t* new_levels,
                               size_t new_levels_size)
{
  if ((new_levels_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_and_universe(handle, universe, &source_state, &universe_state);

    if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
    {
      if (!new_levels)
      {
        set_universe_terminating(universe_state, kTerminateWithoutRemoving);
        disable_pap_data(universe_state);
      }

      // Do this last.
      update_levels_and_or_pap(source_state, universe_state, new_levels, new_levels_size, NULL, 0, kDisableForceSync);
    }

    sacn_unlock();
  }
}

/**
 * @brief Copies the universe's DMX levels and per-address priorities into packets that are sent on the next threaded or
 * manual update.
 *
 * This function will update the outgoing packet data for both DMX and per-address priority data, and reset the logic
 * that slows down packet transmission due to inactivity.
 *
 * The application should adhere to the rules for per-address priority (PAP) specified in @ref per_address_priority.
 * This API will adhere to the rules within the scope of the implementation. This includes handling transmission
 * suppression and the order in which DMX and PAP packets are sent. This also includes automatically setting levels to
 * 0, even if the application specified a different level, for each slot that the application assigns a PAP of 0 (by
 * setting the PAP to 0 or reducing the PAP count).
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_levels A buffer of DMX levels to copy from. If this pointer is NULL, the source will terminate DMX
 * transmission without removing the universe.
 * @param[in] new_levels_size Size of new_levels. This must be no larger than #DMX_ADDRESS_COUNT.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This will only be sent when DMX is also
 * being sent. Setting this to NULL will stop the transmission of per-address priorities, in which case receivers will
 * revert to the universe priority after PAP times out.
 * @param[in] new_priorities_size Size of new_priorities. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_levels_and_pap(sacn_source_t handle, uint16_t universe, const uint8_t* new_levels,
                                       size_t new_levels_size, const uint8_t* new_priorities,
                                       size_t new_priorities_size)
{
  if ((new_levels_size <= DMX_ADDRESS_COUNT) && (new_priorities_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_and_universe(handle, universe, &source_state, &universe_state);

    if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
    {
      if (!new_levels)
        set_universe_terminating(universe_state, kTerminateWithoutRemoving);
      if (!new_levels || !new_priorities)
        disable_pap_data(universe_state);

      // Do this last.
      update_levels_and_or_pap(source_state, universe_state, new_levels, new_levels_size,
                               new_levels ? new_priorities : NULL, new_priorities_size, kDisableForceSync);
    }

    sacn_unlock();
  }
}

/**
 * @brief Like sacn_source_update_levels(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet data to be sent on the next threaded or manual update, and will reset
 * the logic that slows down packet transmission due to inactivity. Additionally, the packet to be sent will have its
 * force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to sacn_source_update_levels().
 *
 * @todo At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_levels A buffer of DMX levels to copy from. If this pointer is NULL, the source will terminate DMX
 * transmission without removing the universe.
 * @param[in] new_levels_size Size of new_levels. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_levels_and_force_sync(sacn_source_t handle, uint16_t universe, const uint8_t* new_levels,
                                              size_t new_levels_size)
{
  if ((new_levels_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_and_universe(handle, universe, &source_state, &universe_state);

    if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
    {
      if (!new_levels)
      {
        set_universe_terminating(universe_state, kTerminateWithoutRemoving);
        disable_pap_data(universe_state);
      }

      // Do this last.
      update_levels_and_or_pap(source_state, universe_state, new_levels, new_levels_size, NULL, 0, kEnableForceSync);
    }

    sacn_unlock();
  }
}

/**
 * @brief Like sacn_source_update_levels_and_pap(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet data to be sent on the next threaded or manual update, and will reset
 * the logic that slows down packet transmission due to inactivity. Additionally, both packets to be sent by this call
 * will have their force_synchronization option flags set.
 *
 * The application should adhere to the rules for per-address priority (PAP) specified in @ref per_address_priority.
 * This API will adhere to the rules within the scope of the implementation. This includes handling transmission
 * suppression and the order in which DMX and PAP packets are sent. This also includes automatically setting levels to
 * 0, even if the application specified a different level, for each slot that the application assigns a PAP of 0 (by
 * setting the PAP to 0 or reducing the PAP count).
 *
 * If no synchronization universe is configured, this function acts like a direct call to
 * sacn_source_update_levels_and_pap().
 *
 * @todo At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_levels A buffer of DMX levels to copy from. If this pointer is NULL, the source will terminate DMX
 * transmission without removing the universe.
 * @param[in] new_levels_size Size of new_levels. This must be no larger than #DMX_ADDRESS_COUNT.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This will only be sent when DMX is also
 * being sent. Setting this to NULL will stop the transmission of per-address priorities, in which case receivers will
 * revert to the universe priority after PAP times out.
 * @param[in] new_priorities_size Size of new_priorities. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_levels_and_pap_and_force_sync(sacn_source_t handle, uint16_t universe,
                                                      const uint8_t* new_levels, size_t new_levels_size,
                                                      const uint8_t* new_priorities, size_t new_priorities_size)
{
  if ((new_levels_size <= DMX_ADDRESS_COUNT) && (new_priorities_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_and_universe(handle, universe, &source_state, &universe_state);

    if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
    {
      if (!new_levels)
        set_universe_terminating(universe_state, kTerminateWithoutRemoving);
      if (!new_levels || !new_priorities)
        disable_pap_data(universe_state);

      // Do this last.
      update_levels_and_or_pap(source_state, universe_state, new_levels, new_levels_size,
                               new_levels ? new_priorities : NULL, new_priorities_size, kEnableForceSync);
    }

    sacn_unlock();
  }
}

/**
 * @brief Trigger the transmission of sACN packets for all universes of sources that were created with
 * manually_process_source set to true.
 *
 * Note: Unless you created the source with manually_process_source set to true, similar functionality will be
 * automatically called by an internal thread of the module. Otherwise, this must be called at the maximum rate
 * at which the application will send sACN (see tick_mode for details of what is actually sent in a call).
 *
 * Sends the current data for universes which have been updated, and sends keep-alive data for universes which
 * haven't been updated. If levels are processed (see tick_mode), this also destroys sources & universes that have been
 * marked for termination after sending the required three terminated packets.
 *
 * @param[in] tick_mode Specifies whether to process levels (and by extension termination) and/or PAP.
 * @return Current number of manual sources tracked by the library, including sources that have been destroyed but are
 * still sending termination packets. This can be useful on shutdown to track when destroyed sources have finished
 * sending the terminated packets and actually been destroyed.
 */
int sacn_source_process_manual(sacn_source_tick_mode_t tick_mode)
{
  return take_lock_and_process_sources(kProcessManualSources, tick_mode);
}

/**
 * @brief Resets the underlying network sockets for all universes of all sources.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the source API will be limited to (the list passed into sacn_init(), if any, is
 * overridden for the source API, but not the other APIs). Then all universes of all sources will be configured to use
 * all of those interfaces.
 *
 * After this call completes successfully, all universes of all sources are considered to be updated and have new levels
 * and priorities. It's as if every source just started sending levels on all their universes.
 *
 * If this call fails, the caller must call sacn_source_destroy() on all sources, because the source API may be in an
 * invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the source API will be
 * limited to, and the status codes are filled in.  If NULL, the source API is allowed to use all available system
 * interfaces.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_reset_networking(const SacnNetintConfig* sys_netint_config)
{
  etcpal_error_t result = kEtcPalErrOk;

  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      sacn_sockets_reset_source(sys_netint_config);

      for (size_t i = 0; (result == kEtcPalErrOk) && (i < get_num_sources()); ++i)
      {
        SacnSource* source = get_source(i);
        if (SACN_ASSERT_VERIFY(source))
        {
          clear_source_netints(source);

          for (size_t j = 0; (result == kEtcPalErrOk) && (j < source->num_universes); ++j)
            result = reset_source_universe_networking(source, &source->universes[j], NULL);
        }
        else
        {
          result = kEtcPalErrSys;
        }
      }

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Resets the underlying network sockets and determines network interfaces for each universe of each source.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the source API will be limited to (the list passed into sacn_init(), if any, is
 * overridden for the source API, but not the other APIs). Then the network interfaces are specified for each universe
 * of each source.
 *
 * After this call completes successfully, all universes of all sources are considered to be updated and have new levels
 * and priorities. It's as if every source just started sending levels on all their universes.
 *
 * If this call fails, the caller must call sacn_source_destroy() on all sources, because the source API may be in an
 * invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in for each universe.  This will only return #kEtcPalErrNoNetints if none of the interfaces
 * work for a universe.
 *
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the source API will be
 * limited to, and the status codes are filled in.  If NULL, the source API is allowed to use all available system
 * interfaces.
 * @param[in, out] per_universe_netint_lists Lists of interfaces the application wants to use for each universe. Must
 * not be NULL. Must include all universes of all sources, and nothing more. The status codes are filled in whenever
 * SacnSourceUniverseNetintList::netints is non-NULL.
 * @param[in] num_per_universe_netint_lists The size of netint_lists. Must not be 0.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided for a universe were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_reset_networking_per_universe(const SacnNetintConfig* sys_netint_config,
                                                         const SacnSourceUniverseNetintList* per_universe_netint_lists,
                                                         size_t num_per_universe_netint_lists)
{
  etcpal_error_t result = kEtcPalErrOk;

  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  if ((per_universe_netint_lists == NULL) || (num_per_universe_netint_lists == 0))
    result = kEtcPalErrInvalid;

  if (result == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      // Validate netint_lists. It must include all universes of all sources and nothing more.
      size_t total_num_universes = 0;
      for (size_t i = 0; (result == kEtcPalErrOk) && (i < get_num_sources()); ++i)
      {
        const SacnSource* source = get_source(i);
        if (SACN_ASSERT_VERIFY(source))
        {
          for (size_t j = 0; (result == kEtcPalErrOk) && (j < source->num_universes); ++j)
          {
            // Universes being removed should not be factored into this.
            if (source->universes[j].termination_state != kTerminatingAndRemoving)
            {
              ++total_num_universes;

              bool found = false;
              get_per_universe_netint_lists_index(source->handle, source->universes[j].universe_id,
                                                  per_universe_netint_lists, num_per_universe_netint_lists, &found);

              if (!found)
                result = kEtcPalErrInvalid;
            }
          }
        }
        else
        {
          result = kEtcPalErrSys;
        }
      }

      if ((result == kEtcPalErrOk) && (num_per_universe_netint_lists != total_num_universes))
        result = kEtcPalErrInvalid;

      if (result == kEtcPalErrOk)
        sacn_sockets_reset_source(sys_netint_config);

      for (size_t i = 0; (result == kEtcPalErrOk) && (i < get_num_sources()); ++i)
      {
        SacnSource* source = get_source(i);
        if (SACN_ASSERT_VERIFY(source))
        {
          clear_source_netints(source);

          for (size_t j = 0; (result == kEtcPalErrOk) && (j < source->num_universes); ++j)
          {
            if (source->universes[j].termination_state == kTerminatingAndRemoving)
            {
              // Keep the universe netints as they are, but add them to source netints again since it was cleared.
              for (size_t k = 0; (result == kEtcPalErrOk) && (k < source->universes[j].netints.num_netints); ++k)
                result = add_sacn_source_netint(source, &source->universes[j].netints.netints[k]);
            }
            else
            {
              // Replace the universe netints, then add the new ones to the source netints.
              size_t list_index =
                  get_per_universe_netint_lists_index(source->handle, source->universes[j].universe_id,
                                                      per_universe_netint_lists, num_per_universe_netint_lists, NULL);

              SacnNetintConfig universe_netint_config;
              universe_netint_config.netints = per_universe_netint_lists[list_index].netints;
              universe_netint_config.num_netints = per_universe_netint_lists[list_index].num_netints;
              universe_netint_config.no_netints = per_universe_netint_lists[list_index].no_netints;
              result = reset_source_universe_networking(source, &source->universes[j], &universe_netint_config);
            }
          }
        }
        else
        {
          result = kEtcPalErrSys;
        }
      }

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

/**
 * @brief Obtain a list of a universe's network interfaces.
 *
 * @param[in] handle Handle to the source that includes the universe.
 * @param[in] universe The universe for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the universe. If this is greater than netints_size, then only
 * netints_size entries were written to the netints array. If the source or universe were not found, 0 is returned.
 */
size_t sacn_source_get_network_interfaces(sacn_source_t handle, uint16_t universe, EtcPalMcastNetintId* netints,
                                          size_t netints_size)
{
  size_t total_num_network_interfaces = 0;

  if (sacn_lock())
  {
    // Look up universe state
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    if (lookup_source_and_universe(handle, universe, &source_state, &universe_state) == kEtcPalErrOk)
    {
      if (universe_state && (universe_state->termination_state != kTerminatingAndRemoving))
        total_num_network_interfaces = get_source_universe_netints(universe_state, netints, netints_size);
    }

    sacn_unlock();
  }

  return total_num_network_interfaces;
}

#endif  // SACN_SOURCE_ENABLED || DOXYGEN

size_t get_per_universe_netint_lists_index(sacn_source_t source, uint16_t universe,
                                           const SacnSourceUniverseNetintList* per_universe_netint_lists,
                                           size_t num_per_universe_netint_lists, bool* found)
{
  if (!SACN_ASSERT_VERIFY(source != SACN_SOURCE_INVALID) || !SACN_ASSERT_VERIFY(per_universe_netint_lists))
    return 0;

  for (size_t i = 0; i < num_per_universe_netint_lists; ++i)
  {
    if ((source == per_universe_netint_lists[i].handle) && (universe == per_universe_netint_lists[i].universe))
    {
      if (found)
        *found = true;

      return i;
    }
  }

  if (found)
    *found = false;

  return 0;
}
