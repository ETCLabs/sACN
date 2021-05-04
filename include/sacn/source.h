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
 * @file sacn/source.h
 * @brief sACN Source API definitions
 *
 * Functions and definitions for the @ref sacn_source "sACN Source API" are contained in this
 * header.
 */

#ifndef SACN_SOURCE_H_
#define SACN_SOURCE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/uuid.h"
#include "sacn/common.h"

/**
 * @defgroup sacn_source sACN Source
 * @ingroup sACN
 * @brief The sACN Source API; see @ref using_source.
 *
 * Components that send sACN are referred to as sACN Sources. Use this API to act as an sACN Source.
 *
 * See @ref using_source for a detailed description of how to use this API.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A handle to a sACN source. */
typedef int sacn_source_t;
/** An invalid sACN source handle value. */
#define SACN_SOURCE_INVALID -1

/**
 * @brief Constant for "infinite" when sending sACN universes.
 *
 * When using dynamic memory, this constant can be passed in when creating a source.
 * It represents an infinite number of universes that can be sent to.
 */
#define SACN_SOURCE_INFINITE_UNIVERSES 0

/** The default keep-alive interval for sources, in milliseconds. */
#define SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT 800

/** A set of configuration information for a sACN source. */
typedef struct SacnSourceConfig
{
  /********* Required values **********/

  /** The source's CID. */
  EtcPalUuid cid;
  /** The source's name, a UTF-8 encoded string. */
  const char* name;

  /********* Optional values **********/

  /** The maximum number of universes this source will send to.  May be #SACN_SOURCE_INFINITE_UNIVERSES.
      This parameter is ignored when configured to use static memory -- #SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE is used
      instead. */
  size_t universe_count_max;

  /** If false (default), this source will be added to a background thread that will send sACN updates at a
      maximum rate of every 23 ms. If true, the source will not be added to the thread and the application
      must call sacn_source_process_manual() at its maximum DMX rate, typically 23 ms. */
  bool manually_process_source;

  /** What IP networking the source will support.  The default is #kSacnIpV4AndIpV6. */
  sacn_ip_support_t ip_supported;

  /** The interval at which the source will send keep-alive packets during transmission suppression, in milliseconds.
      The default is #SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT. */
  int keep_alive_interval;
} SacnSourceConfig;

/** A default-value initializer for an SacnSourceConfig struct. */
#define SACN_SOURCE_CONFIG_DEFAULT_INIT                                             \
  {                                                                                 \
    kEtcPalNullUuid, NULL, SACN_SOURCE_INFINITE_UNIVERSES, false, kSacnIpV4AndIpV6, \
        SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT                                     \
  }

void sacn_source_config_init(SacnSourceConfig* config);

/** A set of configuration information for a sACN universe. */
typedef struct SacnSourceUniverseConfig
{
  /********* Required values **********/

  /** The universe number. At this time, only values from 1 - 63999 are accepted.
      You cannot have a source send more than one stream of values to a single universe. */
  uint16_t universe;

  /********* Optional values **********/

  /** The sACN universe priority that is sent in each packet. This is only allowed to be from 0 - 200. Defaults to 100.
   */
  uint8_t priority;

  /** If true, this sACN source will send preview data. Defaults to false. */
  bool send_preview;

  /** If true, this sACN source will only send unicast traffic on this universe. Defaults to false. */
  bool send_unicast_only;

  /** The initial set of unicast destinations for this universe. This can be changed further by using
      sacn_source_add_unicast_destination() and sacn_source_remove_unicast_destination(). */
  const EtcPalIpAddr* unicast_destinations;
  /** The size of unicast_destinations. */
  size_t num_unicast_destinations;

  /** If non-zero, this is the synchronization universe used to synchronize the sACN output. Defaults to 0.
      TODO: At this time, synchronization is not supported by this library. */
  uint16_t sync_universe;

} SacnSourceUniverseConfig;

/** A default-value initializer for an SacnSourceUniverseConfig struct. */
#define SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT \
  {                                              \
    0, 100, false, false, NULL, 0, 0             \
  }

void sacn_source_universe_config_init(SacnSourceUniverseConfig* config);

/** A set of network interfaces for a particular universe. */
typedef struct SacnSourceUniverseNetintList
{
  /** The source's handle. */
  sacn_source_t handle;
  /** The ID of the universe. */
  uint16_t universe;

  /** If non-NULL, this is the list of interfaces the application wants to use, and the status codes are filled in. If
      NULL, all available interfaces are tried. */
  SacnMcastInterface* netints;
  /** The size of netints, or 0 if netints is NULL. */
  size_t num_netints;
} SacnSourceUniverseNetintList;

etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle);
void sacn_source_destroy(sacn_source_t handle);

etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name);

etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                        SacnMcastInterface* netints, size_t num_netints);
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe);
size_t sacn_source_get_universes(sacn_source_t handle, uint16_t* universes, size_t universes_size);

etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest);
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest);
size_t sacn_source_get_unicast_destinations(sacn_source_t handle, uint16_t universe, EtcPalIpAddr* destinations,
                                            size_t destinations_size);

etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint16_t universe, uint8_t new_priority);
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, uint16_t universe, bool new_preview_flag);
etcpal_error_t sacn_source_change_synchronization_universe(sacn_source_t handle, uint16_t universe,
                                                           uint16_t new_sync_universe);

etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen);
etcpal_error_t sacn_source_send_synchronization(sacn_source_t handle, uint16_t universe);

void sacn_source_update_values(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                               size_t new_values_size);
void sacn_source_update_values_and_pap(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                       size_t new_values_size, const uint8_t* new_priorities,
                                       size_t new_priorities_size);
void sacn_source_update_values_and_force_sync(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                              size_t new_values_size);
void sacn_source_update_values_and_pap_and_force_sync(sacn_source_t handle, uint16_t universe,
                                                      const uint8_t* new_values, size_t new_values_size,
                                                      const uint8_t* new_priorities, size_t new_priorities_size);

int sacn_source_process_manual(void);

etcpal_error_t sacn_source_reset_networking(SacnMcastInterface* netints, size_t num_netints);
etcpal_error_t sacn_source_reset_networking_per_universe(const SacnSourceUniverseNetintList* netint_lists,
                                                         size_t num_netint_lists);

size_t sacn_source_get_network_interfaces(sacn_source_t handle, uint16_t universe, EtcPalMcastNetintId* netints,
                                          size_t netints_size);

void set_fps(uint16_t fps);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_SOURCE_H_ */
