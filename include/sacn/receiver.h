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
 * \file sacn/receiver.h
 * \brief sACN Receiver API definitions
 *
 * Functions and definitions for the \ref sacn_receiver "sACN Receiver API" are contained in this
 * header.
 */

#ifndef SACN_RECEIVER_H_
#define SACN_RECEIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "etcpal/netint.h"
#include "etcpal/uuid.h"
#include "sacn/common.h"

/*!
 * \defgroup sacn_receiver sACN Receiver
 * \ingroup sACN
 * \brief The sACN Receiver API
 *
 * Components that receive sACN are referred to as sACN Receivers. Use this API to act as an sACN
 * Receiver.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to an sACN receiver. */
typedef int sacn_receiver_t;
/*! An invalid sACN receiver handle value. */
#define SACN_RECEIVER_INVALID -1

/*!
 * \brief Constant for "infinite" when listening or merging sACN universes.
 *
 * When using dynamic memory, this constant can be passed in when creating a receiver or a merger.
 * It represents an infinite number of sources on that universe.
 */
#define SACN_RECEIVER_INFINITE_SOURCES 0


/*! An identifier for a version of the sACN standard. */
typedef enum
{
  kSacnStandardVersionNone,      /*!< Neither the draft nor the ratified sACN version. */
  kSacnStandardVersionDraft,     /*!< The 2006 draft sACN standard version. */
  kSacnStandardVersionPublished, /*!< The current published sACN standard version. */
  kSacnStandardVersionAll        /*!< Both the current published and 2006 draft sACN standard version. */
} sacn_standard_version_t;

/*!
 * \brief The default expired notification wait time.
 *
 * Also referred to as a "hold last look" time, the default amount of time the library will wait
 * after a universe enters a data loss condition before calling the sources_lost() callback. Can be
 * changed with sacn_receiver_set_expired_wait().
 */
#define SACN_DEFAULT_EXPIRED_WAIT_MS 1000u

/*! Information about a remote sACN source being tracked by a receiver. */
typedef struct SacnRemoteSource
{
  /*! The Component Identifier (CID) of the source. */
  EtcPalUuid cid;
  /*! The name of the source. */
  char name[SACN_SOURCE_NAME_MAX_LEN];
} SacnRemoteSource;

/*! Information about a sACN source that was found. */
typedef struct SacnFoundSource
{
  /*! The Component Identifier (CID) of the source. */
  EtcPalUuid cid;
  /*! The name of the source. */
  char name[SACN_SOURCE_NAME_MAX_LEN];
  /*! The address from which we received these initial packets. */
  EtcPalSockAddr source_addr;
  /*! The per-universe priority. */
  uint8_t priority;
  /*! The DMX (startcode 0) data. */
  uint8_t values[DMX_ADDRESS_COUNT];
  /*! The count of valid values. */
  size_t values_len;
  /*! The per-address priority (startcode 0xdd) data, if the source is sending it. */
  uint8_t per_address[DMX_ADDRESS_COUNT];
  /*! The count of valid priorities. */
  size_t per_address_len;
} SacnFoundSource;

/*! Information about a sACN source that was lost. */
typedef struct SacnLostSource
{
  /*! The Component Identifier (CID) of the source. */
  EtcPalUuid cid;
  /*! The name of the source. */
  char name[SACN_SOURCE_NAME_MAX_LEN];
  /*! Whether the source was determined to be lost due to the Stream_Terminated bit being set in the
   *  sACN data packet. */
  bool terminated;
} SacnLostSource;

/*!
 * \name sACN receiver flags
 * Valid values for the flags member in the SacnReceiverConfig struct.
 * @{
 */
/*! Filter preview data. If set, any sACN data with the Preview flag set will be dropped for this
 *  universe and sources sending only Preview data will not be tracked. */
#define SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA 0x1
/*!
 * @}
 */

/*!
 * \brief Notify that one or more sources have been found.
 *
 * New sources have been found that can fit in the current collection.  The DMX data and per-address priorities for each
 * source may be acted upon immediately, as the library has determined the correct starting values.  Additionally, the
 * library has waited for a "sampling period" upon startup to make sure the starting set of sources is consistent.
 *
 * After this callback returns, packets for this source will be sent to the sACNUniverseDataCallback().
 *
 * \param[in] handle Handle to the receiver instance for which sources were found.
 * \param[in] found_sources Array of structs describing the source or sources that have been found with their current
 * values.
 * \param[in] num_sources_found Size of the found_sources array.
 * \param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourcesFoundCallback)(sacn_receiver_t handle, const SacnFoundSource* found_sources,
                                         size_t num_found_sources, void* context);

/*!
 * \brief Notify that a data packet has been received.
 *
 * Will be called for every sACN data packet received on a listening universe for a found source, unless the
 * Stream_Terminated bit is set or if preview packets are being filtered.
 *
 * The callback will only be called for packets whose sources have been found via SacnSourcesFoundCallback(), and have
 * not been lost via SacnSourcesLostCallback().
 *
 * \param[in] handle Handle to the receiver instance for which universe data was received.
 * \param[in] source_addr The network address from which the sACN packet originated.
 * \param[in] header The header data of the sACN packet.
 * \param[in] pdata Pointer to the data buffer. Size of the buffer is indicated by header->slot_count.
 * \param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnUniverseDataCallback)(sacn_receiver_t handle, const EtcPalSockAddr* source_addr,
                                         const SacnHeaderData* header, const uint8_t* pdata, void* context);

/*!
 * \brief Notify that one or more sources have entered a data loss state.
 *
 * This could be due to timeout or explicit termination. Sources are grouped using an algorithm
 * designed to prevent level jumps when multiple sources are lost simultaneously. See
 * \ref data_loss_behavior for more information.
 *
 * \param[in] handle Handle to the receiver instance for which sources were lost.
 * \param[in] lost_sources Array of structs describing the source or sources that have been lost.
 * \param[in] num_lost_sources Size of the lost_sources array.
 * \param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourcesLostCallback)(sacn_receiver_t handle, const SacnLostSource* lost_sources,
                                        size_t num_lost_sources, void* context);

/*!
 * \brief Notify that a source has stopped transmission of per-channel priority packets.
 *
 * If #SACN_ETC_PRIORITY_EXTENSION was defined to 0 when sACN was compiled, this callback will
 * never be called and may be set to NULL. This is only called due to a timeout condition; a
 * termination bit is treated as the termination of the entire stream and will result in a
 * sources_lost() notification.
 *
 * \param[in] handle Handle to the receiver instance for which a source stopped sending per-channel
 *                   priority.
 * \param[in] source Information about the source that has stopped transmission of per-channel
 *                   priority.
 * \param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourcePcpLostCallback)(sacn_receiver_t handle, const SacnRemoteSource* source, void* context);

/*!
 * \brief Notify that more than #SACN_RECEIVER_TOTAL_MAX_SOURCES sources are currently sending on
 *        universes being listened to.
 *
 * If #SACN_DYNAMIC_MEM was defined to 1 when sACN was compiled (the default on non-embedded
 * platforms), this callback will never be called and may be set to NULL.
 *
 * This callback is rate-limited: it will only be called when the first sACN packet is received
 * from a source beyond the limit specified by #SACN_RECEIVER_TOTAL_MAX_SOURCES. After that, it will
 * not be called again until the number of sources sending drops below that limit and then hits
 * it again.
 *
 * \param[in] handle Handle to the receiver instance for which the source limit has been exceeded.
 * \param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourceLimitExceededCallback)(sacn_receiver_t handle, void* context);

/*! A set of callback functions that the library uses to notify the application about sACN events. */
typedef struct SacnRecvCallbacks
{
  SacnSourcesFoundCallback sources_found;                /*!< Required */
  SacnUniverseDataCallback universe_data;                /*!< Required */
  SacnSourcesLostCallback sources_lost;                  /*!< Required */
  SacnSourcePcpLostCallback source_pcp_lost;             /*!< Optional */
  SacnSourceLimitExceededCallback source_limit_exceeded; /*!< Optional */
} SacnReceiverCallbacks;

/*! A set of configuration information for an sACN receiver. */
typedef struct SacnReceiverConfig
{
  /********* Required values **********/

  /*! Universe number on which to listen for sACN. */
  uint16_t universe_id;
  /*! The callbacks this receiver will use to notify the application of events. */
  SacnReceiverCallbacks callbacks;

  /********* Optional values **********/

  /*! The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used instead.*/
  size_t source_count_max; 
  /*! A set of option flags. See "sACN receiver flags". */
  unsigned int flags;
  /*! Pointer to opaque data passed back with each callback. */
  void* callback_context;
  /*! (optional) array of network interfaces on which to listen to the specified universe. If NULL,
   *  all available network interfaces will be used. */
  const SacnMcastNetintId* netints;
  /*! Number of elements in the netints array. */
  size_t num_netints;
} SacnReceiverConfig;

/*! A default-value initializer for an SacnReceiverConfig struct. */
#define SACN_RECEIVER_CONFIG_DEFAULT_INIT               \
  {                                                     \
    0, {NULL, NULL, NULL, NULL, NULL}, 0, 0, NULL, NULL, 0 \
  }

void sacn_receiver_config_init(SacnReceiverConfig* config);

etcpal_error_t sacn_receiver_create(const SacnReceiverConfig* config, sacn_receiver_t* handle);
etcpal_error_t sacn_receiver_destroy(sacn_receiver_t handle);
etcpal_error_t sacn_receiver_change_universe(sacn_receiver_t handle, uint16_t new_universe_id);

void sacn_receiver_set_standard_version(sacn_standard_version_t version);
sacn_standard_version_t sacn_receiver_get_standard_version();
void sacn_receiver_set_expired_wait(uint32_t wait_ms);
uint32_t sacn_receiver_get_expired_wait();

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* SACN_RECEIVER_H_ */
