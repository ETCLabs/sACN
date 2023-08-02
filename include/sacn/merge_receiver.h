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

/**
 * @file sacn/merge_receiver.h
 * @brief sACN Merge Receiver API definitions
 *
 * Functions and definitions for the @ref sacn_merge_receiver "sACN Merge Receiver API" are contained in this header.
 */

#ifndef SACN_MERGE_RECEIVER_H_
#define SACN_MERGE_RECEIVER_H_

#include <stdbool.h>
#include <stdint.h>
#include "sacn/receiver.h"
#include "sacn/dmx_merger.h"

/**
 * @defgroup sacn_merge_receiver sACN Merge Receiver
 * @ingroup sACN
 * @brief The sACN Merge Receiver API; see @ref using_merge_receiver.
 *
 * This API is used to minimally wrap the sACN Receiver and DMX Merger logic together so an application can receive and
 * merge sACN sources in software.
 *
 * See @ref using_merge_receiver for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A handle to an sACN Merge Receiver. */
typedef int sacn_merge_receiver_t;
/** An invalid sACN merge_receiver handle value. */
#define SACN_MERGE_RECEIVER_INVALID SACN_RECEIVER_INVALID

/**
 * Newly updated merged data within the configured footprint.
 */
typedef struct SacnRecvMergedData
{
  /**
   * The sACN Universe identifier. Valid range is 1-63999, inclusive.
   */
  uint16_t universe_id;
  /**
   * The range of slots represented by this data (the configured footprint).
   */
  SacnRecvUniverseSubrange slot_range;
  /**
   * The merged levels for the universe at the location indicated by slot_range. This buffer is owned by the library.
   */
  const uint8_t* levels;
  /**
   * The merged per-address priorities for the universe at the location indicated by slot_range. This buffer is owned by
   * the library.
   */
  const uint8_t* priorities;
  /**
   * The source handles of the owners of the slots within slot_range.  If a value in the buffer is
   * #SACN_REMOTE_SOURCE_INVALID, the corresponding slot is not currently controlled. This buffer is owned by the
   * library.
   */
  const sacn_remote_source_t* owners;
  /**
   * The handles of all sources considered to be active on the current universe. Sources that are currently in a
   * sampling period are not represented in the merged data and therefore aren't listed here either. This buffer is
   * owned by the library.
   */
  const sacn_remote_source_t* active_sources;
  /**
   * The current number of sources considered to be active on the current universe (and thus the size of
   * active_sources).
   */
  size_t num_active_sources;
} SacnRecvMergedData;

/**
 * @brief Notify that a new data packet has been received and merged.
 *
 * This callback will be called in multiple ways:
 * 1. When a new non-preview data packet or per-address priority packet is received from the sACN Receiver module,
 * it is immediately and synchronously passed to a DMX Merger. If the sampling period has not ended for the source,
 * the merged result is not passed to this callback until the sampling period ends. Otherwise, it is immediately and
 * synchronously passed to this callback.
 * 2. When a sACN source is no longer sending non-preview data or per-address priority packets, the lost source callback
 * from the sACN Receiver module will be passed to a merger, after which the merged result is passed to this callback
 * pending the sampling period.
 *
 * After a networking reset, some of the sources on the universe may not be included in the resulting sampling period.
 * Therefore, expect this to continue to be called during said sampling period.
 *
 * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
 * packets on the universe.
 *
 * @param[in] handle The handle to the merge receiver instance.
 * @param[in] merged_data The merged data (and relevant information about that data), starting from the first slot
 * of the currently configured footprint. Only sources that are not currently part of a sampling period are part of the
 * merged result.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverMergedDataCallback)(sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data,
                                                    void* context);

/**
 * @brief Notify that a non-data packet has been received.
 *
 * When an established source sends a sACN data packet that doesn't contain DMX values or priorities, the raw data
 * within the configured footprint is immediately and synchronously passed to this callback.
 *
 * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
 * packets on the universe.
 *
 * If the source is sending sACN Sync packets, this callback will only be called when the sync packet is received,
 * if the source forces the packet, or if the source sends a data packet without a sync universe.
 *
 * @todo This version of the sACN library does not support sACN Sync. This paragraph will be valid in the future.
 *
 * @param[in] receiver_handle The handle to the merge receiver instance.
 * @param[in] source_addr The network address from which the sACN packet originated.
 * @param[in] source_info Information about the source that sent this data.
 * @param[in] universe_data The universe data (and relevant information about that data), starting from the first slot
 * of the currently configured footprint.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverNonDmxCallback)(sacn_merge_receiver_t receiver_handle,
                                                const EtcPalSockAddr* source_addr, const SacnRemoteSource* source_info,
                                                const SacnRecvUniverseData* universe_data, void* context);

/**
 * @brief Notify that one or more sources have entered a source loss state.
 *
 * This could be due to timeout or explicit termination. When reset networking is called, the sources on the
 * removed/lost interfaces will time out, and will eventually be included in this notification.
 *
 * Sources are grouped using an algorithm designed to prevent level jumps when multiple sources are lost simultaneously.
 * See @ref source_loss_behavior for more information.
 *
 * @param[in] handle Handle to the merge receiver instance for which sources were lost.
 * @param[in] universe The universe this merge receiver is monitoring.
 * @param[in] lost_sources Array of structs describing the source or sources that have been lost.
 * @param[in] num_lost_sources Size of the lost_sources array.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverSourcesLostCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                     const SacnLostSource* lost_sources, size_t num_lost_sources,
                                                     void* context);

/**
 * @brief Notify that a merge receiver's sampling period has begun.
 *
 * If this sampling period was due to a networking reset, some sources may not be included in it. The sources that
 * are not part of the sampling period will continue to be included in merged data notifications.
 *
 * @param[in] handle Handle to the merge receiver instance for which the sampling period started.
 * @param[in] universe The universe this merge receiver is monitoring.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverSamplingPeriodStartedCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                               void* context);

/**
 * @brief Notify that a merge receiver's sampling period has ended.
 *
 * All sources that were included in this sampling period will now officially be included in merged data
 * notifications. If there was a networking reset during this sampling period, another sampling period may have been
 * scheduled, in which case this will be immediately followed by a sampling period started notification.
 *
 * If there were any active levels received during the sampling period, they were factored into the merged data
 * notification called immediately before this notification. If the merged data notification wasn't called before
 * this notification, that means there currently isn't any active data on the universe.
 *
 * @param[in] handle Handle to the merge receiver instance for which the sampling period ended.
 * @param[in] universe The universe this merge receiver is monitoring.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverSamplingPeriodEndedCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                             void* context);

/**
 * @brief Notify that a source has stopped transmission of per-address priority packets.
 *
 * If #SACN_ETC_PRIORITY_EXTENSION was defined to 0 when sACN was compiled, this callback will
 * never be called and may be set to NULL. This is only called due to a timeout condition; a
 * termination bit is treated as the termination of the entire stream and will result in a
 * sources_lost() notification.
 *
 * @param[in] handle Handle to the merge receiver instance for which a source stopped sending per-address
 *                   priority.
 * @param[in] universe The universe this merge receiver is monitoring.
 * @param[in] source Information about the source that has stopped transmission of per-address
 *                   priority.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverSourcePapLostCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                       const SacnRemoteSource* source, void* context);

/**
 * @brief Notify that more than the configured maximum number of sources are currently sending on
 *        the universe being listened to.
 *
 * This is a notification that is directly forwarded from the sACN Receiver module.
 *
 * @param[in] handle Handle to the merge receiver instance for which the source limit has been exceeded.
 * @param[in] universe The universe this merge receiver is monitoring.
 * @param[in] context Context pointer that was given at the creation of the merge receiver instance.
 */
typedef void (*SacnMergeReceiverSourceLimitExceededCallback)(sacn_merge_receiver_t handle, uint16_t universe,
                                                             void* context);

/** A set of callback functions that the library uses to notify the application about sACN events. */
typedef struct SacnMergeReceiverCallbacks
{
  SacnMergeReceiverMergedDataCallback universe_data;                      /**< Required */
  SacnMergeReceiverNonDmxCallback universe_non_dmx;                       /**< Optional */
  SacnMergeReceiverSourcesLostCallback sources_lost;                      /**< Optional */
  SacnMergeReceiverSamplingPeriodStartedCallback sampling_period_started; /**< Optional */
  SacnMergeReceiverSamplingPeriodEndedCallback sampling_period_ended;     /**< Optional */
  SacnMergeReceiverSourcePapLostCallback source_pap_lost;                 /**< Optional */
  SacnMergeReceiverSourceLimitExceededCallback source_limit_exceeded;     /**< Optional */
  void* callback_context; /**< (optional) Pointer to opaque data passed back with each callback. */
} SacnMergeReceiverCallbacks;

/** A set of configuration information for an sACN merge receiver. */
typedef struct SacnMergeReceiverConfig
{
  /********* Required values **********/

  /** Universe number on which to listen for sACN. */
  uint16_t universe_id;
  /** The callbacks this merge receiver will use to notify the application of events. */
  SacnMergeReceiverCallbacks callbacks;

  /********* Optional values **********/

  /** The footprint within the universe to monitor. TODO: Currently unimplemented and thus ignored. */
  SacnRecvUniverseSubrange footprint;

  /** The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
      This parameter is ignored when configured to use static memory -- the lower of
      #SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER or #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used instead.*/
  int source_count_max;

  /** If true, this allows per-address priorities (if any are received) to be fed into the merger. If false, received
      per-address priorities are ignored, and only universe priorities are used in the merger. Keep in mind that this
      setting will be ignored if #SACN_ETC_PRIORITY_EXTENSION = 0, in which case per-address priorities are ignored. */
  bool use_pap;

  /** What IP networking the merge_receiver will support.  The default is #kSacnIpV4AndIpV6. */
  sacn_ip_support_t ip_supported;
} SacnMergeReceiverConfig;

/** A default-value initializer for an SacnMergeReceiverConfig struct. */
#define SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT                                                                 \
  {                                                                                                             \
    0, {NULL, NULL, NULL, NULL}, {1, DMX_ADDRESS_COUNT}, SACN_RECEIVER_INFINITE_SOURCES, true, kSacnIpV4AndIpV6 \
  }

/** A set of network interfaces for a particular merge receiver. */
typedef struct SacnMergeReceiverNetintList
{
  /** The merge receiver's handle. */
  sacn_merge_receiver_t handle;

  /** If non-NULL, this is the list of interfaces the application wants to use, and the status codes are filled in. If
      NULL, all available interfaces are tried. */
  SacnMcastInterface* netints;
  /** The size of netints, or 0 if netints is NULL. */
  size_t num_netints;
  /** If this is true, this merge receiver will not use any network interfaces for multicast traffic. */
  bool no_netints;
} SacnMergeReceiverNetintList;

/** Information about a remote sACN source being tracked by a merge receiver. */
typedef struct SacnMergeReceiverSource
{
  /** The handle of the source. */
  sacn_remote_source_t handle;
  /** The Component Identifier (CID) of the source. */
  EtcPalUuid cid;
  /** The name of the source. */
  char name[SACN_SOURCE_NAME_MAX_LEN];
  /** The network address from which the most recent sACN packet originated. */
  EtcPalSockAddr addr;
} SacnMergeReceiverSource;

void sacn_merge_receiver_config_init(SacnMergeReceiverConfig* config);

etcpal_error_t sacn_merge_receiver_create(const SacnMergeReceiverConfig* config, sacn_merge_receiver_t* handle,
                                          const SacnNetintConfig* netint_config);
etcpal_error_t sacn_merge_receiver_destroy(sacn_merge_receiver_t handle);
etcpal_error_t sacn_merge_receiver_get_universe(sacn_merge_receiver_t handle, uint16_t* universe_id);
etcpal_error_t sacn_merge_receiver_get_footprint(sacn_merge_receiver_t handle, SacnRecvUniverseSubrange* footprint);
etcpal_error_t sacn_merge_receiver_change_universe(sacn_merge_receiver_t handle, uint16_t new_universe_id);
etcpal_error_t sacn_merge_receiver_change_footprint(sacn_merge_receiver_t handle,
                                                    const SacnRecvUniverseSubrange* new_footprint);
etcpal_error_t sacn_merge_receiver_change_universe_and_footprint(sacn_merge_receiver_t handle, uint16_t new_universe_id,
                                                                 const SacnRecvUniverseSubrange* new_footprint);
etcpal_error_t sacn_merge_receiver_reset_networking(const SacnNetintConfig* sys_netint_config);
etcpal_error_t sacn_merge_receiver_reset_networking_per_receiver(
    const SacnNetintConfig* sys_netint_config, const SacnMergeReceiverNetintList* per_receiver_netint_lists,
    size_t num_per_receiver_netint_lists);
size_t sacn_merge_receiver_get_network_interfaces(sacn_merge_receiver_t handle, EtcPalMcastNetintId* netints,
                                                  size_t netints_size);
etcpal_error_t sacn_merge_receiver_get_source(sacn_merge_receiver_t merge_receiver_handle,
                                              sacn_remote_source_t source_handle, SacnMergeReceiverSource* source_info);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_MERGE_RECEIVER_H_ */
