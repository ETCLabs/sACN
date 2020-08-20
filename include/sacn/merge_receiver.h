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

/*!
 * \file sacn/merge_receiver.h
 * \brief sACN Merge Receiver API definitions
 *
 * Functions and definitions for the \ref sacn_merge_receiver "sACN Merge Receiver API" are contained in this header.
 */

#ifndef SACN_MERGE_RECEIVER_H_
#define SACN_MERGE_RECEIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include <sacn/receiver.h>
#include <sacn/dmx_merger.h>

/*!
 * \defgroup sacn_merge_receiver sACN Merge Receiver
 * \ingroup sACN
 * \brief The sACN Merge Receiver API
 *
 * This API is used to minimally wrap the sACN Receiver and DMX Merger logic together.
 * Its implementation can also be used as a sample for how to start integrating the sACN Receiver and DMX Merger APIs
 * into your application.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to an sACN Merge Receiver. */
typedef int sacn_merge_receiver_t;
/*! An invalid sACN merge_receiver handle value. */
#define SACN_MERGE_RECEIVER_INVALID -1

/*!
 * \brief Notify that a new data packet has been received and merged.
 *
 * This callback will be called in multiple ways:
 * 1. When a new non-preview data packet or per-address priority packet is received from the sACN Receiver module,
 * it is immediately and synchronously passed to the DMX Merger, after which the merged result is immediately and
 * synchronously passed to this callback.  Note that this includes the data received from the
 * SacnSourcesFoundCallback().
 * 2. When a sACN source is no longer sending non-preview data or per-address priority packets, the lost source callback
 * from the sACN Receiver module will be passed to the merger, after which the merged result is immediately and
 * synchronously passed to this callback.
 *
 * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
 * packets on the universe.
 *
 * \param[in] handle The handle to the merge receiver instance.
 * \param[in] universe The universe number this merge receiver is monitoring.
 * \param[in] slots Buffer of #DMX_ADDRESS_COUNT bytes containing the merged levels for the universe. This buffer is
 * owned by the library.
 * \param[in] slot_owners Buffer of #DMX_ADDRESS_COUNT source_ids.  If a value in the buffer is
 *           #DMX_MERGER_SOURCE_INVALID, the corresponding slot is not currently controlled. You can also use
 *            SACN_DMX_MERGER_IS_SOURCE_VALID(slot_owners, index) to check the slot validity. This buffer is owned by
 * the library.
 * \param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverMergedDataCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                    const uint8_t* slots, const source_id_t* slot_owners,
                                                    void* context);

/*!
 * \brief Notify that a non-data packet has been received.
 *
 * When an established source sends a sACN data packet that doesn't contain DMX values or priorities, the raw packet is
 * immediately and synchronously passed to this callback.
 *
 * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
 * packets on the universe.
 *
 * If the source is sending sACN Sync packets, this callback will only be called when the sync packet is received,
 * if the source forces the packet, or if the source sends a data packet without a sync universe.
 * TODO: We still need add support for sACN Sync.
 *
 * \param[in] handle The handle to the merge receiver instance.
 * \param[in] universe The universe number this merge receiver is monitoring.
 * \param[in] source_addr The network address from which the sACN packet originated.
 * \param[in] header The header data of the sACN packet.
 * \param[in] pdata Pointer to the data buffer. Size of the buffer is indicated by header->slot_count. This buffer is
 * owned by the library.
 * \param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverNonDMXCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                const EtcPalSockAddr* source_addr, const SacnHeaderData* header,
                                                const uint8_t* pdata, void* context);

/*!
 * \brief Notify that more than the configured maximum number of sources are currently sending on
 *        the universe being listened to.
 *
 * This is a notification that is directly forwarded from the sACN Receiver module.
 *
 * \param[in] handle Handle to the merge receiver instance for which the source limit has been exceeded.
 * \param[in] universe The universe number this merge receiver is monitoring.
 * \param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverSourceLimitExceededCallback)(sacn_merge_receiver_t handle, uint16_t universe, void* context);

/*! A set of callback functions that the library uses to notify the application about sACN events. */
typedef struct SacnMergeReceiverCallbacks
{
  SacnMergeReceiverMergedDataCallback universe_data;                  /*!< Required */
  SacnMergeReceiverNonDMXCallback universe_non_dmx;                   /*!< Required */
  SacnMergeReceiverSourceLimitExceededCallback source_limit_exceeded; /*!< Optional */
  void* callback_context; /*!< (optional) Pointer to opaque data passed back with each callback. */
} SacnMergeReceiverCallbacks;

/*! A set of configuration information for an sACN merge receiver. */
typedef struct SacnMergeReceiverConfig
{
  /********* Required values **********/

  /*! Universe number on which to listen for sACN. */
  uint16_t universe_id;
  /*! The callbacks this merge receiver will use to notify the application of events. */
  SacnMergeReceiverCallbacks callbacks;

  /********* Optional values **********/

  /*! The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used
     instead.*/
  size_t source_count_max;
  /*! (optional) array of network interfaces on which to listen to the specified universe. If NULL,
   *  all available network interfaces will be used. */
  const SacnMcastNetintId* netints;
  /*! Number of elements in the netints array. */
  size_t num_netints;
} SacnMergeReceiverConfig;

/*! A default-value initializer for an SacnMergeReceiverConfig struct. */
#define SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT \
  {                                             \
    0, {NULL, NULL, NULL}, 0, NULL, NULL, 0     \
  }

void sacn_merge_receiver_config_init(SacnMergeReceiverConfig* config);

etcpal_error_t sacn_merge_receiver_create(const SacnMergeReceiverConfig* config, sacn_merge_receiver_t* handle);
etcpal_error_t sacn_merge_receiver_destroy(sacn_merge_receiver_t handle);
etcpal_error_t sacn_merge_receiver_get_universe(sacn_merge_receiver_t handle, uint16_t* universe_id);
etcpal_error_t sacn_merge_receiver_change_universe(sacn_merge_receiver_t handle, uint16_t new_universe_id);
etcpal_error_t sacn_merge_receiver_reset_networking(sacn_merge_receiver_t handle, const SacnMcastNetintId* netints,
                                                    size_t num_netints);
source_id_t sacn_merge_receiver_get_source_id(sacn_merge_receiver_t handle, const EtcPalUuid* source_cid);
etcpal_error_t sacn_merge_receiver_get_source_cid(sacn_merge_receiver_t handle, source_id_t source_id,
                                                  EtcPalUuid* source_cid);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* SACN_MERGE_RECEIVER_H_ */
