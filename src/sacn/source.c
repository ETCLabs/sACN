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
 - Make sure I didn't miss any requirements or details from the protocol.
 - A whole lotta implementation.  You should be able to base some this on the earlier C++ library, but also look at the
 spec for 0xdd, since the old library didn't handle intermixing 0x00 & 0xdd on take-control and release-control.  Ray
 can help there, too. This will probably be the largest portion of testing..
 - Get usage/API documentation in place and cleaned up so we can have a larger review.
 - Make sure everything works with static & dynamic memory.
 - This entire project should build without warnings!!
 - Make an example source that uses the new api.
 - Make an example source & testing for the c++ header.
 - Don't forget the Draft library!!!
 - IPv6 support.
 - Sync support.  Update TODO comments in source.h & .c that state sync isn't supported.
*/

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
  // TODO CHRISTIAN
  return kEtcPalErrOk;
}

void sacn_source_deinit(void)
{
  // TODO CHRISTIAN
  // Shut down the Tick thread...
}

/*!
 * \brief Initialize an sACN Source Config struct to default values.
 *
 * \param[out] config Config struct to initialize.
 * \param[in] cid The CID to assign. Must not be NULL.
 * \param[in] name The source name to assign. Will be truncated to fit #SACN_SOURCE_NAME_MAX_LEN bytes.
 */
void sacn_source_config_init(SacnSourceConfig* config, const EtcPalUuid* cid, const char* name)
{
  // TODO CHRISTIAN
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(cid);
  ETCPAL_UNUSED_ARG(name);
}

/*!
 * \brief Initialize an sACN Source Universe Config struct to default values.
 *
 * \param[out] config Config struct to initialize.
 * \param[in] universe The universe number to create.
 * \param[in] values_buffer The DMX values buffer, may not be NULL.
 * \param[in] values_len The length of values_buffer.
 * \param[in] priorities_buffer If non-NULL, holds the per-address priority buffer.
 */
void sacn_source_universe_config_init(SacnSourceUniverseConfig* config, uint16_t universe, const uint8_t* values_buffer,
                                      size_t values_len, const uint8_t* priorities_buffer)
{
  // TODO CHRISTIAN
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(values_buffer);
  ETCPAL_UNUSED_ARG(values_len);
  ETCPAL_UNUSED_ARG(priorities_buffer);
}

/*!
 * \brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source, but no data is sent until sacn_source_add_universe() and
 * sacn_source_set_dirty() is called.
 *
 * Note that a source is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * \param[in] config Configuration parameters for the sACN source to be created.
 * \param[out] handle Filled in on success with a handle to the sACN source.
 * \param[in, out] ifaces Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If NULL, all available interfaces are tried.
 * \param[in, out] ifaces_count Optional. The size of ifaces, or 0 if ifaces is NULL.
 * \return #kEtcPalErrOk: Source successfully created.
 * \return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate an additional source.
 * \return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle,
                                  SacnMcastInterfaceToUse* ifaces, size_t ifaces_count)
{
  // TODO CHRISTIAN
  // If the Tick thread hasn't been started yet, start it if the config isn't manual.

  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(ifaces);
  ETCPAL_UNUSED_ARG(ifaces_count);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the
 * packet for use in displaying the identity of a source to a user." Only up to
 * #SACN_SOURCE_NAME_MAX_LEN characters will be used.
 *
 * \param[in] handle Handle to the source to change.
 * \param[in] new_name New name to use for this universe.
 * \return #kEtcPalErrOk: Name set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name)
{
  // TODO CHRISTIAN
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_name);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs
 * on a call to sacn_source_process_sources() after an additional three packets have been sent with the
 * "Stream_Terminated" option set. The source will also stop transmitting sACN universe discovery packets.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to sacn_source_add_universe().
 *
 * \param[in] handle Handle to the source to destroy.
 */
void sacn_source_destroy(sacn_source_t handle)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
}

/*!
 * \brief Add a universe to an sACN source.
 *
 * Adds a universe to a source.
 * After this call completes, the applicaton must call sacn_source_set_dirty() to mark it ready for processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe
 * Discovery packets.

 * \param[in] handle Handle to the source to which to add a universe.
 * \param[in] config Configuration parameters for the universe to be added.
 * \return #kEtcPalErrOk: Universe successfully added.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: Universe given was already added to this source.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrNoMem: No room to allocate additional universe.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(config);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove a universe from a source.
 *
 * This queues the source for removal. The destruction actually occurs
 * on a call to sacn_source_process_sources() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * The source will also stop transmitting sACN universe discovery packets for that universe.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to sacn_source_add_universe().
 *
 * \param[in] handle Handle to the source from which to remove the universe.
 * \param[in] universe Universe to remove.
 */
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
}

/*!
 * \brief Add a unicast destination for a source's universe.
 *
 * Adds a unicast destination for a source's universe.
 * After this call completes, the applicaton must call sacn_source_set_dirty() to mark it ready for processing.
 *
 * \param[in] handle Handle to the source to change.
 * \param[in] universe Universe to change.
 * \param[in] dest The destination IP.  May not be NULL.
 * \return #kEtcPalErrOk: Address added successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(dest);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove a unicast destination on a source's universe.
 *
 * This queues the address for removal. The removal actually occurs
 * on a call to sacn_source_process_sources() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * \param[in] handle Handle to the source to change.
 * \param[in] universe Universe to change.
 * \param[in] dest The destination IP.  May not be NULL, and must match the address passed to
 * sacn_source_add_unicast_destination().
 */
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(dest);
}

/*!
 * \brief Change the priority of a universe on a sACN source.
 *
 * \param[in] handle Handle to the source for which to set the priority.
 * \param[in] universe Universe to change.
 * \param[in] new_priority New priority of the data sent from this source. Valid range is 0 to 200,
 *                         inclusive.
 * \return #kEtcPalErrOk: Priority set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint16_t universe, uint8_t new_priority)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_priority);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the sending_preview option on a universe of a sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
 *
 * \param[in] handle Handle to the source for which to set the Preview_Data option.
 * \param[in] new_preview_flag The new sending_preview option.
 * \return #kEtcPalErrOk: sending_preview option set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, uint16_t universe, bool new_preview_flag)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_preview_flag);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Changes the synchronize uinverse for a universe of a sACN source.
 *
 * This will change the synchronization universe used by a sACN universe on the source.
 * If this value is 0, synchronization is turned off for that universe.
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * \param[in] handle Handle to the source to change.
 * \param[in] universe The universe to change.
 * \param[in] new_sync_universe The new synchronization universe to set.
 * \return #kEtcPalErrOk: sync_universe set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
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

/*!
 * \brief Immediately sends the provided sACN start code & data.
 *
 * Immediately sends a sACN packet with the provided start code and data.
 * This function is intended for sACN packets that have a startcode other than 0 or 0xdd, since those
 * start codes are taken care of by sacn_source_process_sources().
 *
 * \param[in] handle Handle to the source.
 * \param[in] universe Universe to send on.
 * \param[in] start_code The start code to send.
 * \param[in] buffer The buffer to send.  Must not be NULL.
 * \param[in] buflen The size of buffer.
 * \return #kEtcPalErrOk: Message successfully sent.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(start_code);
  ETCPAL_UNUSED_ARG(buffer);
  ETCPAL_UNUSED_ARG(buflen);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Immediately sends a synchronization packet for the universe on a source.
 *
 * This will cause an immediate transmission of a synchronization packet for the source/universe.
 * If the universe does not have a synchronization universe configured, this call is ignored.
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * \param[in] handle Handle to the source.
 * \param[in] universe Universe to send on.
 * \return #kEtcPalErrOk: Message successfully sent.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_send_synchronization(sacn_source_t handle, uint16_t universe)
{
  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Indicate that the data in the buffer for this source and universe has changed and
 *        should be sent on the next call to sacn_source_process_sources().
 *
 * \param[in] handle Handle to the source to mark as dirty.
 * \param[in] universe Universe to mark as dirty.
 */
void sacn_source_set_dirty(sacn_source_t handle, uint16_t universe)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
}

/*!
 * \brief Indicate that the data in the buffers for a list of universes on a source  has
 *        changed and should be sent on the next call to sacn_source_process_sources().
 *
 * \param[in] handle Handle to the source.
 * \param[in] universes Array of universes to mark as dirty. Must not be NULL.
 * \param[in] num_universes Size of the universes array.
 */
void sacn_source_set_list_dirty(sacn_source_t handle, const uint16_t* universes, size_t num_universes)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universes);
  ETCPAL_UNUSED_ARG(num_universes);
}

/*!
 * \brief Like sacn_source_set_dirty, but also sets the force_sync flag on the packet.
 *
 * This function indicates that the data in the buffer for this source and universe has changed,
 * and should be sent on the next call to sacn_source_process_sources().  Additionally, the packet
 * to be sent will have its force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to sacn_source_set_dirty().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * \param[in] handle Handle to the source to mark as dirty.
 * \param[in] universe Universe to mark as dirty.
 */
void sacn_source_set_dirty_and_force_sync(sacn_source_t handle, uint16_t universe)
{
  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
}

/*!
 * \brief Process created sources and do the actual sending of sACN data on all universes.
 *
 * Note: Unless you created the source with manually_process_source set to true, this will be automatically
 * called by an internal thread of the module. Otherwise, this must be called at the maximum rate
 * at which the application will send sACN.
 *
 * Sends data for universes which have been marked dirty, and sends keep-alive data for universes which
 * haven't changed. Also destroys sources & universes that have been marked for termination after sending the required
 * three terminated packets.
 *
 * \return Current number of sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and actually
 *         been destroyed.
 */
size_t sacn_source_process_sources(void)
{
  // TODO CHRISTIAN
  return 0;
}

/*!
 * \brief Resets the underlying network sockets for the sACN source.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, all universes on a source are considered to be dirty and have
 * new values and priorities. It's as if the source just started sending values on that universe.
 *
 * If this call fails, the caller must call sacn_source_destroy(), because the source may be in an
 * invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * \param[in] handle Handle to the source for which to reset the networking.
 * \param[in, out] ifaces Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If NULL, all available interfaces are tried.
 * \param[in, out] ifaces_count Optional. The size of ifaces, or 0 if ifaces is NULL.
 * \return #kEtcPalErrOk: Source changed successfully.
 * \return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_reset_networking(sacn_source_t handle, SacnMcastInterfaceToUse* ifaces, size_t ifaces_count)
{
  // TODO CHRISTIAN
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(ifaces);
  ETCPAL_UNUSED_ARG(ifaces_count);

  return kEtcPalErrNotImpl;
}
