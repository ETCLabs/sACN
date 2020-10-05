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
 - Get usage/API documentation in place and cleaned up so we can have a larger review.
 - Make sure everything works with static & dynamic memory.
 --------------NICK CLEAN UP
 - I've added the universe to all the callbacks.  Make sure the notification structs are initialized and work correctly for all notifications.
 - Add full support for the sources found notification. Packets aren't forwarded to the application until the source list is stable.
 - Add unicast support to sockets.c in the SACN_RECEIVER_SOCKET_PER_UNIVERSE case.
 - Make sure unicast support works in both socket modes, with one or more receivers created.
 - Make sure everything works with static & dynamic memory.
 - Make source addition honors source_count_max, even in dynamic mode.
 - Start Codes that aren't 0 & 0xdd should still get forwarded to the application in handle_sacn_data_packet!
 - refactor common.c's init & deinit functions to be more similar to https://github.com/ETCLabs/RDMnet/blob/develop/src/rdmnet/core/common.c#L141's functions, as Sam put in the review. 
 - Make the example receiver use the new api.
 - Make an example receiver & testing for the c++ header.
 - IPv6 support.  See the CHRISTIAN TODO IPV6 comments for some hints on where to change.
 - Make sure draft support works properly.  If a source is sending both draft and ratified, the sequence numbers should
   filter out the duplicate packet (just like IPv4 & IPv6).
 - This entire project should build without warnings!!
 - Sync support.  Update TODO comments in receiver & merge_receiver that state sync isn't supported.
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
  //Shut down the Tick thread...
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
  //TODO CHRISTIAN
}

/*!
 * \brief Initialize an sACN Source Universe Config struct to default values.
 *
 * \param[out] config Config struct to initialize.
 * \param[in] universe_id The universe number to create.
 * \param[in] values_buffer The DMX values buffer, may not be NULL.
 * \param[in] values_len The length of values_buffer.
 * \param[in] priorities_buffer If non-NULL, holds the per-address priority buffer.
 */
void sacn_source_universe_config_init(SacnSourceUniverseConfig* config, uint16_t universe_id,
                                      const uint8_t* values_buffer, size_t values_len, const uint8_t* priority_buffer)
{
  // TODO CHRISTIAN
}

/*!
 * \brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source, but no data is sent until sacn_source_add_universe() is called.
 *
 * \param[in] config Configuration parameters for the sACN source to be created.
 * \param[out] handle Filled in on success with a handle to the sACN source.
 * \return #kEtcPalErrOk: Source successfully created.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate additional source.
 * \return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle)
{
  // TODO CHRISTIAN
  //If the Tick thread hasn't been started yet, start it if the config isn't manual.

  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs
 * on a call to sacn_process_sources() after three packets have been sent with the
 * "Stream_Terminated" option set. The source will also stop transmitting sACN universe discovery packets.
 *
 * \param[in] handle Handle to the source to destroy.
 * \return #kEtcPalErrOk: Source destruction successfully queued.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_destroy(sacn_source_t handle)
{
  //TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Add a universe to an sACN source.
 *
 * Adds a universe to a source. If dirty_now is true, the source will start sending values on the
 * next call to sacn_process_sources(). If dirty_now is false, the applicaton must call sacn_source_set_dirty() to mark
 * it ready for processing.
 * Regardless of whether or not the universe is marked dirty, the source will add the universe to its sACN Universe
 * Discovery packets.

 * \param[in] handle Handle to the source to which to add a universe.
 * \param[in] config Configuration parameters for the universe to be added.
 * \param[in] dirty_now Whether or not to immediately mark the universe as dirty.
 * \return #kEtcPalErrOk: Start code successfully added.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: Universe given was already added to this source.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrNoMem: No room to allocate additional universe.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config, bool dirty_now)
{
  // TODO CHRISTIAN

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(dirty_now);
  return kEtcPalErrNotImpl;
}


/*!
 * \brief Remove a universe from a source.
 *
 * If this is not the only start code being sent for this source, simply stops sending data for
 * this start code immediately. Otherwise, if this is the last start code remaining on the
 * source, flags the source for termination; data will stop after three packets are sent from
 * sacn_process_sources() with the "Stream_Terminated" option sent. Either way, the buffer provided
 * for this start code by sacn_source_add_start_code() should be considered invalid after calling
 * this function.
 *
 * \param[in] handle Handle to the source from which to remove the start code.
 * \param[in] start_code Start code to remove.
 * \return #kEtcPalErrOk: Start code removed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the start code was
 *                              not previously added to this source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_remove_universe(sacn_source_t handle, uint16_t universe_id);  // Gracefully shutds down universe (sending values properly), stops universe discovery.
/*!
 * \brief Remove a start code from a universe.
 *
 * If this is not the only start code being sent for this source, simply stops sending data for
 * this start code immediately. Otherwise, if this is the last start code remaining on the
 * source, flags the source for termination; data will stop after three packets are sent from
 * sacn_process_sources() with the "Stream_Terminated" option sent. Either way, the buffer provided
 * for this start code by sacn_source_add_start_code() should be considered invalid after calling
 * this function.
 *
 * \param[in] handle Handle to the source from which to remove the start code.
 * \param[in] start_code Start code to remove.
 * \return #kEtcPalErrOk: Start code removed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the start code was
 *                              not previously added to this source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_remove_start_code(sacn_source_t handle, uint8_t start_code)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(start_code);
  // TODO
  return kEtcPalErrNotImpl;
}

/* SEND NOW NOTES
 * Provides a pointer to a buffer for the slot data for this start code; note that this slot data
 * doesn't include the start code itself. The library retains ownership of this buffer and it is
 * invalidated on sacn_source_remove_start_code() or sacn_source_destroy(). No data is sent until
 * the start code data is marked as dirty using sacn_source_set_dirty() or passed in the
 * dirty_handles parameter to sacn_process_sources().
 */

/*!
 * \brief Change the priority of an sACN source.
 *
 * \param[in] handle Handle to the source for which to set the priority.
 * \param[in] new_priority New priority of the data sent from this source. Valid range is 0 to 200,
 *                         inclusive.
 * \return #kEtcPalErrOk: Priority set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint8_t new_priority)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_priority);
  // TODO
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the Preview_Data option on an sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
 *
 * \param[in] handle Handle to the source for which to set the Preview_Data option.
 * \param[in] new_preview_flag The new Preview_Data option.
 * \return #kEtcPalErrOk: Preview_Data option set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, bool new_preview_flag)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_preview_flag);
  // TODO
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the
 * packet for use in displaying the identity of a source to a user." Only up to
 * #SACN_SOURCE_NAME_MAX_LEN characters will be used.
 *
 * \param[in] handle Handle to the source for which to set the Preview_Data option.
 * \param[in] new_name New name to use for this universe.
 * \return #kEtcPalErrOk: Name set successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_name);
  return kEtcPalErrNotImpl;
  // TODO
}

/*!
 * \brief Indicate that the data in the buffer for this source and start code has changed and
 *        should be sent on the next call to sacn_process_sources().
 *
 * After a source and start code have been created, data will not be sent on the source/start code
 * combination until it has been marked dirty for the first time.
 *
 * \param[in] handle Handle to the source to mark as dirty.
 * \param[in] start_code Start code to mark as dirty.
 */
void sacn_source_set_dirty(sacn_source_t handle, uint8_t start_code)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(start_code);
  // TODO
}

/*!
 * \brief Indicate that the data in the buffers for a set of source/start code combinations has
 *        changed and should be sent on the next call to sacn_process_sources().
 *
 * After a source and start code have been created, data will not be sent on the source/start code
 * combination until it has been marked dirty for the first time.
 *
 * \param[in] start_codes Array of source/start code pairs to mark as dirty.
 * \param[in] num_start_codes Size of the start_codes array.
 */
void sacn_sources_set_dirty(const SacnSourceStartCodePair* start_codes, size_t num_start_codes)
{
  ETCPAL_UNUSED_ARG(start_codes);
  ETCPAL_UNUSED_ARG(num_start_codes);
  // TODO
}

/*!
 * \brief Send the data for a source and start code immediately.
 *
 * This behavior is rarely necessary, but provided for unanticipated use cases and debug use. Use
 * this function to send a sACN packet for a universe immediately (i.e. in between calls to
 * sacn_process_sources()).
 *
 * \param[in] handle Handle to the source for which to send data immediately.
 * \param[in] start_code Start code for which to send data immediately.
 */
void sacn_source_send_now(sacn_source_t handle, uint8_t start_code)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(start_code);
  // TODO
}

/*!
 * \brief Send the data for a set of source/start code combinations immediately.
 *
 * This behavior is rarely necessary, but provided for unanticipated use cases and debug use. Use
 * this function to send a sACN packet for a universe immediately (i.e. in between calls to
 * sacn_process_sources()).
 *
 * Note that calling this function could result in behavior which violates E1.31-2016 &sect; 6.6.
 *
 * \param[in] start_codes Array of source/start code pairs on which to send data immediately.
 * \param[in] num_start_codes Size of the start_codes array.
 */
void sacn_sources_send_now(const SacnSourceStartCodePair* start_codes, size_t num_start_codes)
{
  ETCPAL_UNUSED_ARG(start_codes);
  ETCPAL_UNUSED_ARG(num_start_codes);
  // TODO
}

/*!
 * \brief Process created sources and do the actual sending of sACN data.
 *
 * Must be called at the maximum rate at which the application will send sACN. Sends data for
 * start codes which have been marked dirty, and sends keep-alive data for start codes which are
 * static. Also destroys sources that have been marked for termination after sending the required
 * three terminated packets.
 *
 * \return Current number of sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and actually
 *         been destroyed.
 */
size_t sacn_process_sources(void)
{
  // TODO
  return 0;
}

