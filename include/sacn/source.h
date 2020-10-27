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
 * @brief The sACN Source API.
 *
 * Components that send sACN are referred to as sACN Sources. Use this API to act as an sACN Source.
 * 
 * Usage:
 * @code
 * #include "sacn/source.h"
 * 
 * EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
 * // Initialize log_params...
 * 
 * etcpal_error_t init_result = sacn_init(&log_params);
 * // Or, to init without worrying about logs from the sACN library...
 * etcpal_error_t init_result = sacn_init(NULL);
 * 
 * EtcPalUuid my_cid;
 * char my_name[SACN_SOURCE_NAME_MAX_LEN];
 * // Assuming my_cid and my_name are initialized by the application...
 * 
 * SacnSourceConfig my_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
 * sacn_source_config_init(&my_config, &my_cid, my_name);
 * 
 * SacnMcastInterface my_netints[NUM_MY_NETINTS];
 * // Assuming my_netints and NUM_MY_NETINTS are initialized by the application...
 * 
 * sacn_source_t my_handle;
 * 
 * // If you want to specify specific network interfaces to use:
 * sacn_source_create(&my_config, &my_handle, my_netints, NUM_MY_NETINTS);
 * // Or, if you just want to use all network interfaces:
 * sacn_source_create(&my_config, &my_handle, NULL, 0);
 * 
 * uint8_t my_values_buffer[DMX_ADDRESS_COUNT];
 * uint8_t my_priorities_buffer[DMX_ADDRESS_COUNT];
 * 
 * uint16_t my_universe = 1;  // Using universe 1 as an example.
 * 
 * SacnSourceUniverseConfig my_universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
 * sacn_source_universe_config_init(&my_universe_config, my_universe, my_values_buffer, DMX_ADDRESS_COUNT,
 *                                  my_priorities_buffer);
 * // Or, if you don't want to send per-address priorities:
 * sacn_source_universe_config_init(&my_universe_config, my_universe, my_values_buffer, DMX_ADDRESS_COUNT, NULL);
 * my_universe_config.priority = 123;
 * 
 * sacn_source_add_universe(my_handle, &my_universe_config);
 * // You can add additional universes as well, in the same way.
 * 
 * // Initialize my_values_buffer and (possibly) my_priorities_buffer with the values you want to send...
 * 
 * // Now set the universe to dirty. Then the source thread will handle transmitting the data (unless you set
 * // manually_process_source to true in the SacnSourceConfig).
 * sacn_source_set_dirty(my_handle, my_universe);
 * 
 * // Unicast can be sent to one or more addresses, in addition to multicast.
 * EtcPalIpAddr custom_destination;  // Application initializes custom_destination...
 * sacn_source_add_unicast_destination(my_handle, my_universe, &custom_destination);
 * sacn_source_set_dirty(my_handle, my_universe); // Indicate the data should be sent on multicast and unicast (or just
 *                                                // unicast if send_unicast_only is enabled in SacnSourceUniverseConfig).
 * 
 * // Custom start code data can also be sent immediately:
 * uint8_t my_custom_start_code;
 * uint8_t my_custom_start_code_data[DMX_ADDRESS_COUNT];
 * // Initialize start code and data...
 * sacn_source_send_now(my_handle, my_universe, my_custom_start_code, my_custom_start_code_data, DMX_ADDRESS_COUNT);
 * 
 * // You can also set up a synchronization universe for a universe.
 * // Receivers should hang on to the data and wait for a sync message.
 * uint16_t my_sync_universe = 123;  // Let's say the sync universe is 123, for example.
 * sacn_source_change_synchronization_universe(my_handle, my_universe, my_sync_universe);
 * 
 * // Whenever you want the data to be applied, you can immediately send a sync message for your universe.
 * sacn_source_send_synchronization(my_handle, my_universe);
 * 
 * // The preview flag, priority, and name can also be changed at any time:
 * const char* new_name = "Hello World";
 * sacn_source_change_name(my_handle, new_name)
 * uint8_t new_priority = 50;
 * sacn_source_change_priority(my_handle, my_universe, new_priority);
 * bool new_preview_flag = true;
 * sacn_source_change_preview_flag(my_handle, my_universe, new_preview_flag);
 * sacn_source_set_dirty(my_handle, my_universe);  // Indicate new data should be transmitted.
 * 
 * // You can remove a unicast destination previously added:
 * sacn_source_remove_unicast_destination(my_handle, my_universe, &custom_destination);
 * // Or remove a universe from your source:
 * sacn_source_remove_universe(my_handle, my_universe);
 * // Or destroy the source altogether:
 * sacn_source_destroy(my_handle);
 * 
 * // During application shutdown, everything can be cleaned up by calling sacn_deinit.
 * sacn_deinit();
 * @endcode
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

/** A set of configuration information for a sACN source. */
typedef struct SacnSourceConfig
{
  /********* Required values **********/

  /** The source's CID. */
  EtcPalUuid cid;
  /** The source's name, a UTF-8 encoded string. */
  char name[SACN_SOURCE_NAME_MAX_LEN];

  /********* Optional values **********/

  /** The maximum number of universes this source will send to.  May be #SACN_SOURCE_INFINITE_UNIVERSES.
      This parameter is ignored when configured to use static memory -- #SACN_SOURCE_MAX_UNIVERSES is used instead.*/
  size_t universe_count_max;

  /** If false (default), this module starts a shared thread that calls sacn_source_process_all() every 23 ms.
      If true, no thread is started and the application must call sacn_source_process_all() at its DMX rate,
      usually 23 ms. */
  bool manually_process_source;

} SacnSourceConfig;

/** A default-value initializer for an SacnSourceConfig struct. */
#define SACN_SOURCE_CONFIG_DEFAULT_INIT \
  {                                     \
    kEtcPalNullUuid, "", 0, false       \
  }

void sacn_source_config_init(SacnSourceConfig* config, const EtcPalUuid* cid, const char* name);

typedef struct SacnSourceUniverseConfig
{
  /********* Required values **********/

  /** The universe number. At this time, only values from 1 - 63999 are accepted.
      You cannot have a source send more than one stream of values to a single universe. */
  uint16_t universe;
  /** The buffer of up to 512 dmx values that will be sent each tick.
      This pointer may not be NULL. The memory is owned by the application, and should not
      be destroyed until after the universe is deleted on this source. */
  const uint8_t* values_buffer;
  /** The size of values_buffer. */
  size_t num_values;

  /********* Optional values **********/

  /** The sACN universe priority that is sent in each packet. This is only allowed to be from 0 - 200. Defaults to 100. */
  uint8_t priority;
  /** The (optional) buffer of up to 512 per-address priorities that will be sent each tick.
      If this is NULL, only the universe priority will be used.
      If non-NULL, this buffer is evaluated each tick.  Changes to and from 0 ("don't care") cause appropriate
      sacn packets over time to take and give control of those DMX values as defined in the per-address priority
      specification.
      The memory is owned by the application, and should not be destroyed until after the universe is
      deleted on this source. The size of this buffer must match the size of values_buffer.  */
  const uint8_t* priorities_buffer; 

  /** If true, this sACN source will send preview data. Defaults to false. */
  bool send_preview;

  /** If true, this sACN source will only send unicast traffic on this universe. Defaults to false. */
  bool send_unicast_only;

  /** If non-zero, this is the synchronization universe used to synchronize the sACN output. Defaults to 0. */
  uint16_t sync_universe;

} SacnSourceUniverseConfig;

/** A default-value initializer for an SacnSourceUniverseConfig struct. */
#define SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT \
  {                                              \
    0, NULL, 0, 100, NULL, false, false, 0       \
  }

void sacn_source_universe_config_init(SacnSourceUniverseConfig* config, uint16_t universe, const uint8_t* values_buffer,
                                      size_t values_len, const uint8_t* priorities_buffer);

etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle,
                                  SacnMcastInterface* netints, size_t num_netints);
void sacn_source_destroy(sacn_source_t handle);

etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name);

etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config);
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe);

etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest);
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest);

etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint16_t universe, uint8_t new_priority);
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, uint16_t universe, bool new_preview_flag);
etcpal_error_t sacn_source_change_synchronization_universe(sacn_source_t handle, uint16_t universe,
                                                           uint16_t new_sync_universe);

etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen);
etcpal_error_t sacn_source_send_synchronization(sacn_source_t handle, uint16_t universe);

void sacn_source_set_dirty(sacn_source_t handle, uint16_t universe);
void sacn_source_set_list_dirty(sacn_source_t handle, const uint16_t* universes, size_t num_universes);
void sacn_source_set_dirty_and_force_sync(sacn_source_t handle, uint16_t universe);

int sacn_source_process_all(void);

etcpal_error_t sacn_source_reset_networking(sacn_source_t handle, SacnMcastInterface* netints, size_t num_netints);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_SOURCE_H_ */
