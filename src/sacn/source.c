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

#include "sacn/source.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/****************************** Private macros *******************************/

/****************************** Private types ********************************/

/**************************** Private variables ******************************/

/*********************** Private function prototypes *************************/

/*************************** Function definitions ****************************/

/* Initialize the sACN Source module. Internal function called from sacn_init().
   This also starts up the module-provided Tick thread. */
etcpal_error_t sacn_source_init(void)
{
  return kEtcPalErrOk;
}

void sacn_source_deinit(void)
{
  // Shut down the Tick thread...
}

/**
 * @brief Initialize an sACN Source Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_config_init(SacnSourceConfig* config)
{
  ETCPAL_UNUSED_ARG(config);
}

/**
 * @brief Initialize an sACN Source Universe Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_universe_config_init(SacnSourceUniverseConfig* config)
{
  ETCPAL_UNUSED_ARG(config);
}

/**
 * @brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source, but no data is sent until sacn_source_add_universe() and
 * either sacn_source_update_values() or sacn_source_update_values_and_pap() are called.
 *
 * @param[in] config Configuration parameters for the sACN source to be created.
 * @param[out] handle Filled in on success with a handle to the sACN source.
 * @return #kEtcPalErrOk: Source successfully created.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate an additional source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle)
{
  // If the Tick thread hasn't been started yet, start it if the config isn't manual.

  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the
 * packet for use in displaying the identity of a source to a user." Only up to
 * #SACN_SOURCE_NAME_MAX_LEN characters will be used.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] new_name New name to use for this universe.
 * @return #kEtcPalErrOk: Name set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_name);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs
 * on a call to sacn_source_process_all() after an additional three packets have been sent with the
 * "Stream_Terminated" option set. The source will also stop transmitting sACN universe discovery packets.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to sacn_source_add_universe().
 *
 * @param[in] handle Handle to the source to destroy.
 */
void sacn_source_destroy(sacn_source_t handle)
{
  ETCPAL_UNUSED_ARG(handle);
}

/**
 * @brief Add a universe to an sACN source.
 *
 * Adds a universe to a source.
 * After this call completes, the applicaton must call either sacn_source_update_values() or
 * sacn_source_update_values_and_pap() to mark it ready for processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe
 * Discovery packets.
 *
 * Note that a universe is considered as successfully added if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the source to which to add a universe.
 * @param[in] config Configuration parameters for the universe to be added.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and
 * the status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Universe successfully added.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: Universe given was already added to this source.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or a network interface ID given was not
 * found on the system.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                        SacnMcastInterface* netints, size_t num_netints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Remove a universe from a source.
 *
 * This queues the source for removal. The destruction actually occurs
 * on a call to sacn_source_process_all() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * The source will also stop transmitting sACN universe discovery packets for that universe.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to sacn_source_add_universe().
 *
 * @param[in] handle Handle to the source from which to remove the universe.
 * @param[in] universe Universe to remove.
 */
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
}

/**
 * @brief Add a unicast destination for a source's universe.
 *
 * Adds a unicast destination for a source's universe.
 * After this call completes, the applicaton must call either sacn_source_update_values() or
 * sacn_source_update_values_and_pap() to mark it ready for processing.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL.
 * @return #kEtcPalErrOk: Address added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(dest);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Remove a unicast destination on a source's universe.
 *
 * This queues the address for removal. The removal actually occurs
 * on a call to sacn_source_process_all() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL, and must match the address passed to
 * sacn_source_add_unicast_destination().
 */
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(dest);
}

/**
 * @brief Obtain a list of unicast destinations this source is transmitting on.
 *
 * @param[in] handle Handle to the source for which to obtain the list of unicast destinations.
 * @param[out] destinations A pointer to an application-owned array where the unicast destination list will be written.
 * @param[in] destinations_size The size of the provided destinations array.
 * @return The total number of unicast destinations being transmitted by the source. If this is greater than
 * destinations_size, then only destinations_size addresses were written to the destinations array. If the source was
 * not found, 0 is returned.
 */
size_t sacn_source_get_unicast_destinations(sacn_source_t handle, EtcPalIpAddr* destinations, size_t destinations_size)
{
  return 0;  // TODO
}

/**
 * @brief Change the priority of a universe on a sACN source.
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
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_priority);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Change the send_preview option on a universe of a sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
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
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_preview_flag);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Changes the synchronize uinverse for a universe of a sACN source.
 *
 * This will change the synchronization universe used by a sACN universe on the source.
 * If this value is 0, synchronization is turned off for that universe.
 *
 * TODO: At this time, synchronization is not supported by this library.
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
 * start codes are taken care of by sacn_source_process_all().
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
 */
etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(start_code);
  ETCPAL_UNUSED_ARG(buffer);
  ETCPAL_UNUSED_ARG(buflen);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Indicate that a new synchronization packet should be sent on the given synchronization universe.
 *
 * This will cause the transmission of a synchronization packet for the source on the given synchronization universe.
 *
 * TODO: At this time, synchronization is not supported by this library, so this function is not implemented.
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
 * @brief Copies the universe's dmx values into the packet to be sent on the next call to sacn_source_process_all()
 *
 * This function will update the outgoing packet values, and reset the logic that slows down packet transmission due to
 * inactivity.
 *
 * When you don't have per-address priority changes to make, use this function. Otherwise, use
 * sacn_source_update_values_and_pap().
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512 values
 * will be used.
 * @param[in] new_values_size Size of new_values.
 */
void sacn_source_update_values(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                               size_t new_values_size)
{
  // TODO
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_values);
  ETCPAL_UNUSED_ARG(new_values_size);
}

/**
 * @brief Copies the universe's dmx values and per-address priorities into packets that are sent on the next call to
 * sacn_source_process_all()
 *
 * This function will update the outgoing packet values for both DMX and per-address priority data, and reset the logic
 * that slows down packet transmission due to inactivity.
 *
 * Per-address priority support has specific rules about when to send value changes vs. pap changes.  These rules are
 * documented in https://etclabs.github.io/sACN/docs/head/per_address_priority.html, and are triggered by the use of
 * this function. Changing per-address priorities to and from "don't care", changing the size of the priorities array,
 * or passing in NULL/non-NULL for the priorities will cause this library to do the necessary tasks to "take control" or
 * "release control" of the corresponding DMX values.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512 values
 * will be used.
 * @param[in] new_values_size Size of new_values.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities.
 */
void sacn_source_update_values_and_pap(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                       size_t new_values_size, const uint8_t* new_priorities,
                                       size_t new_priorities_size)
{
  // TODO
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_values);
  ETCPAL_UNUSED_ARG(new_values_size);
  ETCPAL_UNUSED_ARG(new_priorities);
  ETCPAL_UNUSED_ARG(new_priorities_size);
}

/**
 * @brief Like sacn_source_update_values(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next call to sacn_source_process_all(), and
 * will reset the logic that slows down packet transmission due to inactivity. Additionally, the packet to be sent will
 * have its force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to sacn_source_update_values().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512 values
 * will be used.
 * @param[in] new_values_size Size of new_values.
 */
void sacn_source_update_values_and_force_sync(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                              size_t new_values_size)
{
  // TODO
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_values);
  ETCPAL_UNUSED_ARG(new_values_size);
}

/**
 * @brief Like sacn_source_update_values_and_pap(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next call to sacn_source_process_all(), and
 * will reset the logic that slows down packet transmission due to inactivity. Additionally, the final packet to be sent
 * by this call will have its force_synchronization option flag set.
 *
 * Per-address priority support has specific rules about when to send value changes vs. pap changes.  These rules are
 * documented in https://etclabs.github.io/sACN/docs/head/per_address_priority.html, and are triggered by the use of
 * this function. Changing per-address priorities to and from "don't care", changing the size of the priorities array,
 * or passing in NULL/non-NULL for the priorities will cause this library to do the necessary tasks to "take control" or
 * "release control" of the corresponding DMX values.
 *
 * If no synchronization universe is configured, this function acts like a direct call to
 * sacn_source_update_values_and_pap().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512 values
 * will be used.
 * @param[in] new_values_size Size of new_values.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities.
 */
void sacn_source_update_values_and_pap_and_force_sync(sacn_source_t handle, uint16_t universe,
                                                      const uint8_t* new_values, size_t new_values_size,
                                                      const uint8_t* new_priorities, size_t new_priorities_size)
{
  // TODO
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_values);
  ETCPAL_UNUSED_ARG(new_values_size);
  ETCPAL_UNUSED_ARG(new_priorities);
  ETCPAL_UNUSED_ARG(new_priorities_size);
}

/**
 * @brief Trigger the transmision of sACN packets for all universes of sources that were created with
 * manually_process_source set to true.
 *
 * Note: Unless you created the source with manually_process_source set to true, similar functionality will be
 * automatically called by an internal thread of the module. Otherwise, this must be called at the maximum rate
 * at which the application will send sACN.
 *
 * Sends the current data for universes which have been updated, and sends keep-alive data for universes which
 * haven't been updated. Also destroys sources & universes that have been marked for termination after sending the
 * required three terminated packets.
 *
 * @return Current number of sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and actually
 *         been destroyed.
 */
int sacn_source_process_manual(void)
{
  return 0;
}

/**
 * @brief Resets the underlying network sockets for a universe.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the universe is considered to be updated and have new values and priorities.
 * It's as if the source just started sending values on that universe.
 *
 * If this call fails, the caller must call sacn_source_destroy(), because the source may be in an
 * invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the source for which to reset the networking.
 * @param[in] universe Universe to reset netowrk interfaces for.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Source changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or a network interface ID given was not
 * found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_reset_networking(sacn_source_t handle, uint16_t universe, SacnMcastInterface* netints,
                                            size_t num_netints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);

  return kEtcPalErrNotImpl;
}

/**
 * @brief Obtain the statuses of a universe's network interfaces.
 *
 * @param[in] handle Handle to the source that includes the universe.
 * @param[in] universe The universe for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the universe. If this is greater than netints_size, then only
 * netints_size addresses were written to the netints array. If the source or universe were not found, 0 is returned.
 */
size_t sacn_source_get_network_interfaces(sacn_source_t handle, uint16_t universe, SacnMcastInterface* netints,
                                          size_t netints_size)
{
  return 0;  // TODO
}
