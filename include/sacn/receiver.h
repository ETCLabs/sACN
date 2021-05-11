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

/**
 * @file sacn/receiver.h
 * @brief sACN Receiver API definitions
 *
 * Functions and definitions for the @ref sacn_receiver "sACN Receiver API" are contained in this
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

/**
 * @defgroup sacn_receiver sACN Receiver
 * @ingroup sACN
 * @brief The sACN Receiver API; see @ref using_receiver.
 *
 * Components that receive sACN are referred to as sACN Receivers. Use this API to act as an sACN
 * Receiver.
 *
 * See @ref using_receiver for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A handle to an sACN receiver. */
typedef int sacn_receiver_t;
/** An invalid sACN receiver handle value. */
#define SACN_RECEIVER_INVALID -1

/**
 * @brief Constant for "infinite" when listening or merging sACN universes.
 *
 * When using dynamic memory, this constant can be passed in when creating a receiver or a merger.
 * It represents an infinite number of sources on that universe.
 */
#define SACN_RECEIVER_INFINITE_SOURCES 0

/**
 * @brief The default expired notification wait time.
 *
 * The default amount of time the library will wait after a universe enters a source loss condition
 * before calling the sources_lost() callback. Can be changed with sacn_receiver_set_expired_wait().
 */
#define SACN_DEFAULT_EXPIRED_WAIT_MS 1000u

/** Information about a remote sACN source being tracked by a receiver. */
typedef struct SacnRemoteSource
{
  /** The handle of the source. */
  sacn_remote_source_t handle;
  /** The Component Identifier (CID) of the source. */
  EtcPalUuid cid;
  /** The name of the source. */
  char name[SACN_SOURCE_NAME_MAX_LEN];
} SacnRemoteSource;

/** Information about a sACN source that was lost. */
typedef struct SacnLostSource
{
  /** The handle of the source. */
  sacn_remote_source_t handle;
  /** The Component Identifier (CID) of the source. */
  EtcPalUuid cid;
  /** The name of the source. */
  char name[SACN_SOURCE_NAME_MAX_LEN];
  /** If true, the source was determined to be lost due to the Stream_Terminated bit being set in the
   *  sACN data packet. If false, the source was lost due to a source loss timeout. */
  bool terminated;
} SacnLostSource;

/**
 * @name sACN receiver flags
 * Valid values for the flags member in the SacnReceiverConfig struct.
 * @{
 */
/** Filter preview data. If set, any sACN data with the Preview flag set will be dropped for this universe. */
#define SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA 0x1
/**
 * @}
 */

/**
 * @brief Notify that a data packet has been received.
 *
 * This will not be called if the Stream_Terminated bit is set, or if the Preview_Data bit is set and preview packets
 * are being filtered.
 *
 * Start code 0xDD packets will only trigger this notification if #SACN_ETC_PRIORITY_EXTENSION is set to 1. This
 * callback will be called for all other start codes received, even those without a startcode of 0x00 or 0xDD.
 *
 * This notification will not be called for a source until the first NULL start code packet is received. After that
 * happens, this notification is always called immediately during the sampling period, if #SACN_ETC_PRIORITY_EXTENSION
 * is set to 0, or if the start code is not 0x00 or 0xDD. Otherwise, this notification won't be called until both 0x00
 * and 0xDD start codes are received (in which case the 0xDD notification comes first), or the 0xDD timer has expired
 * and a 0x00 packet is received.
 *
 * If the source is sending sACN Sync packets, this callback will only be called when the sync packet is received,
 * if the source forces the packet, or if the source sends a data packet without a sync universe.
 * TODO: this version of the sACN library does not support sACN Sync. This paragraph will be valid in the future.
 *
 * @param[in] receiver_handle Handle to the receiver instance for which universe data was received.
 * @param[in] source_addr The network address from which the sACN packet originated.
 * @param[in] header The header data of the sACN packet.
 * @param[in] pdata Pointer to the data buffer. Size of the buffer is indicated by header->slot_count.
 * @param[in] is_sampling True if this data was received during the sampling period, false otherwise.
 * @param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnUniverseDataCallback)(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                         const SacnHeaderData* header, const uint8_t* pdata, bool is_sampling,
                                         void* context);

/**
 * @brief Notify that one or more sources have entered a source loss state.
 *
 * This could be due to timeout or explicit termination. Sources are grouped using an algorithm
 * designed to prevent level jumps when multiple sources are lost simultaneously. See
 * @ref source_loss_behavior for more information.
 *
 * @param[in] handle Handle to the receiver instance for which sources were lost.
 * @param[in] universe The universe this receiver is monitoring.
 * @param[in] lost_sources Array of structs describing the source or sources that have been lost.
 * @param[in] num_lost_sources Size of the lost_sources array.
 * @param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourcesLostCallback)(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                                        size_t num_lost_sources, void* context);

/**
 * @brief Notify that a receiver's sampling period has begun.
 *
 * @param[in] handle Handle to the receiver instance for which the sampling period started.
 * @param[in] universe The universe this receiver is monitoring.
 * @param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSamplingPeriodStartedCallback)(sacn_receiver_t handle, uint16_t universe, void* context);

/**
 * @brief Notify that a receiver's sampling period has ended.
 *
 * @param[in] handle Handle to the receiver instance for which the sampling period ended.
 * @param[in] universe The universe this receiver is monitoring.
 * @param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSamplingPeriodEndedCallback)(sacn_receiver_t handle, uint16_t universe, void* context);

/**
 * @brief Notify that a source has stopped transmission of per-address priority packets.
 *
 * If #SACN_ETC_PRIORITY_EXTENSION was defined to 0 when sACN was compiled, this callback will
 * never be called and may be set to NULL. This is only called due to a timeout condition; a
 * termination bit is treated as the termination of the entire stream and will result in a
 * sources_lost() notification.
 *
 * @param[in] handle Handle to the receiver instance for which a source stopped sending per-address
 *                   priority.
 * @param[in] universe The universe this receiver is monitoring.
 * @param[in] source Information about the source that has stopped transmission of per-address
 *                   priority.
 * @param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourcePapLostCallback)(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source,
                                          void* context);

/**
 * @brief Notify that more than the configured maximum number of sources are currently sending on
 *        the universe being listened to.
 *
 * If #SACN_DYNAMIC_MEM was defined to 1 when sACN was compiled (the default on non-embedded
 * platforms), and the configuration you pass to sacn_receiver_create() has source_count_max set to
 * #SACN_RECEIVER_INFINITE_SOURCES, this callback will never be called and may be set to NULL.

 * if #SACN_DYNAMIC_MEM was defined to 0 when sACN was compiled, source_count_max is ignored and
 * #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used instead.
 *
 * This callback is rate-limited: it will only be called when the first sACN packet is received
 * from a source beyond the limit specified. After that, it will not be called again until the number of sources sending
 * drops below that limit and then hits it again.
 *
 * @param[in] handle Handle to the receiver instance for which the source limit has been exceeded.
 * @param[in] universe The universe this receiver is monitoring.
 * @param[in] context Context pointer that was given at the creation of the receiver instance.
 */
typedef void (*SacnSourceLimitExceededCallback)(sacn_receiver_t handle, uint16_t universe, void* context);

/** A set of callback functions that the library uses to notify the application about sACN events. */
typedef struct SacnRecvCallbacks
{
  SacnUniverseDataCallback universe_data;                    /**< Required */
  SacnSourcesLostCallback sources_lost;                      /**< Required */
  SacnSamplingPeriodStartedCallback sampling_period_started; /**< Optional */
  SacnSamplingPeriodEndedCallback sampling_period_ended;     /**< Optional */
  SacnSourcePapLostCallback source_pap_lost;                 /**< Optional */
  SacnSourceLimitExceededCallback source_limit_exceeded;     /**< Optional */
  void* context; /**< (optional) Pointer to opaque data passed back with each callback. */
} SacnReceiverCallbacks;

/** A set of configuration information for an sACN receiver. */
typedef struct SacnReceiverConfig
{
  /********* Required values **********/

  /** Universe number on which to listen for sACN. */
  uint16_t universe_id;
  /** The callbacks this receiver will use to notify the application of events. */
  SacnReceiverCallbacks callbacks;

  /********* Optional values **********/

  /** The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used
     instead.*/
  int source_count_max;
  /** A set of option flags. See "sACN receiver flags". */
  unsigned int flags;

  /** What IP networking the receiver will support.  The default is #kSacnIpV4AndIpV6. */
  sacn_ip_support_t ip_supported;
} SacnReceiverConfig;

/** A default-value initializer for an SacnReceiverConfig struct. */
#define SACN_RECEIVER_CONFIG_DEFAULT_INIT                                                        \
  {                                                                                              \
    0, {NULL, NULL, NULL, NULL, NULL, NULL}, SACN_RECEIVER_INFINITE_SOURCES, 0, kSacnIpV4AndIpV6 \
  }

/** A set of network interfaces for a particular receiver. */
typedef struct SacnReceiverNetintList
{
  /** The receiver's handle. */
  sacn_receiver_t handle;

  /** If non-NULL, this is the list of interfaces the application wants to use, and the status codes are filled in. If
      NULL, all available interfaces are tried. */
  SacnMcastInterface* netints;
  /** The size of netints, or 0 if netints is NULL. */
  size_t num_netints;
} SacnReceiverNetintList;

void sacn_receiver_config_init(SacnReceiverConfig* config);

etcpal_error_t sacn_receiver_create(const SacnReceiverConfig* config, sacn_receiver_t* handle,
                                    SacnMcastInterface* netints, size_t num_netints);
etcpal_error_t sacn_receiver_destroy(sacn_receiver_t handle);
etcpal_error_t sacn_receiver_get_universe(sacn_receiver_t handle, uint16_t* universe_id);
etcpal_error_t sacn_receiver_change_universe(sacn_receiver_t handle, uint16_t new_universe_id);
etcpal_error_t sacn_receiver_reset_networking(SacnMcastInterface* netints, size_t num_netints);
etcpal_error_t sacn_receiver_reset_networking_per_receiver(const SacnReceiverNetintList* netint_lists,
                                                           size_t num_netint_lists);
size_t sacn_receiver_get_network_interfaces(sacn_receiver_t handle, EtcPalMcastNetintId* netints, size_t netints_size);

void sacn_receiver_set_expired_wait(uint32_t wait_ms);
uint32_t sacn_receiver_get_expired_wait();

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_RECEIVER_H_ */
