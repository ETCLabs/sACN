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
 * @file sacn/universe_discovery.h
 * @brief sACN Universe Discovery API definitions
 *
 * Functions and definitions for the @ref sacn_universe_discovery "sACN UniverseDiscovery API" are contained in this
 * header.
 */

#ifndef SACN_UNIVERSE_DISCOVERY_H_
#define SACN_UNIVERSE_DISCOVERY_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "etcpal/netint.h"
#include "etcpal/uuid.h"
#include "sacn/common.h"

/**
 * @defgroup sacn_universe_discovery sACN Universe Discovery
 * @ingroup sACN
 * @brief The sACN Universe Discovery API
 *
 * sACN sources often periodically send Universe Discovery packets to announce what universes they are sourcing. Use this API to monitor such traffic.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A handle to an sACN universe discovery listener. */
typedef int sacn_universe_discovery_t;
/** An invalid sACN universe discovery handle value. */
#define SACN_UNIVERSE_DISCOVERY_INVALID -1

/**
 * @brief Constant for "infinite" when listening for sources or universes on a source.
 *
 * When using dynamic memory, this constant can be passed in when creating a universe discovery listener.
 * It represents an infinite number of sources or universes on a source.
 */
#define SACN_UNIVERSE_DISCOVERY_INFINITE 0


/**
 * @brief Notify that a source is new or has changed.
 *
 * This passes the source's current universe list, but you will only get this callback when the module detects
 * that the source is new or the list has somehow changed.
 *
 * The list of sourced universes is guaranteed by the protocol to be numerically sorted.
 *
 * @param[in] handle Handle to the listening universe discovery instance.
 * @param[in] cid The CID of the source.
 * @param[in] name The null-terminated UTF-8 string.
 * @param[in] sourced_universes Numerically sorted array of the currently sourced universes.  Will be NULL if the source
 * is not currently transmitting any universes.
 * @param[in] num_sourced_universe Size of the sourced_universes array.  Will be 0 if the source is not currently
 * transmitting any universes.
 * @param[in] context Context pointer that was given at the creation of the universe discovery instance.
 */
typedef void (*SacnUniverseDiscoveryUpdateSourceCallback)(sacn_universe_discovery_t handle, const EtcPalUuid* cid,
                                                          const char* name, const uint16_t* sourced_universes,
                                                          size_t num_sourced_sources, void* context);

/**
 * @brief Notify that a source is no longer transmitting Universe Discovery messages.
 *
 * @param[in] handle Handle to the listening universe discovery instance.
 * @param[in] cid The CID of the source.
 * @param[in] name The null-terminated UTF-8 string.
 * @param[in] context Context pointer that was given at the creation of the universe discovery instance.
 */
typedef void (*SacnUniverseDiscoverySourceExpiredCallback)(sacn_universe_discovery_t handle, const EtcPalUuid* cid,
                                                           const char* name, void* context);

/**
 * @brief Notify that the module has run out of memory to track universes or sources
 *
 * If #SACN_DYNAMIC_MEM was defined to 1 when sACN was compiled (the default on non-embedded
 * platforms), and the configuration you pass to sacn_receiver_create() has source_count_max and
 * universes_per_source_max set to #SACN_UNIVERSE_DISCOVERY_INFINITE, this callback will never be called and may be set
 * to NULL.
 *
 * if #SACN_DYNAMIC_MEM was defined to 0 when sACN was compiled, source_count_max is ignored and
 * #SACN_UNIVERSE_DISCOVERY_MAX_SOURCES and #SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_SOURCE are used instead.
 *
 * This callback is rate-limited: it will only be called when the first universe discovery packet is received that takes
 * the module beyond a memory limit.  After that, it will not be called until the number of sources or universes has
 * dropped below the limit and hits one again.
 *
 * @param[in] handle Handle to the listening universe discovery instance.
 * @param[in] context Context pointer that was given at the creation of the universe discovery instance.
 */
typedef void (*SacnUniverseDiscoveryLimitExceededCallback)(sacn_universe_discovery_t handle, void* context);


/** A set of callback functions that the library uses to notify the application about universe discovery events. */
typedef struct SacnUniverseDiscoveryCallbacks
{
  SacnUniverseDiscoveryUpdateSourceCallback source_update;   /**< Required */
  SacnUniverseDiscoverySourceExpiredCallback source_expired; /**< Required */
  SacnUniverseDiscoveryLimitExceededCallback limit_exceeded; /**< Optional */
  void* context; /**< (optional) Pointer to opaque data passed back with each callback. */
} SacnUniverseDiscoveryCallbacks;

/** A set of configuration information for a sACN Universe Discovery listener. */
typedef struct SacnUniverseDiscoveryConfig
{
  /** The callbacks this listener will use to notify the application of events. */
  SacnUniverseDiscoveryCallbacks callbacks;

  /********* Optional values **********/

  /** The maximum number of sources this listener will record.  It is recommended that applications using dynamic
     memory use #SACN_UNIVERSE_DISCOVERY_INFINITE for this value. This parameter is ignored when configured to use
     static memory -- #SACN_UNIVERSE_DISCOVERY_MAX_SOURCES is used instead.*/
  size_t source_count_max;

  /** The maximum number of universes this listener will record for a source.  It is recommended that applications using
     dynamic memory use #SACN_UNIVERSE_DISCOVERY_INFINITE for this value. This parameter is ignored when configured to
     use static memory -- #SACN_UNIVERSE_DISCOVERY_MAX_SOURCES is used instead.*/
  size_t universes_per_source_max;
} SacnUniverseDiscoveryConfig;

/** A default-value initializer for an SacnUniverseDiscoveryConfig struct. */
#define SACN_UNIVERSE_DISCOVERY_CONFIG_DEFAULT_INIT                                                  \
  {                                                                                                  \
    {NULL, NULL, NULL, NULL}, SACN_UNIVERSE_DISCOVERY_INFINITE, SACN_UNIVERSE_DISCOVERY_INFINITE,    \
  }

void sacn_universe_discovery_config_init(SacnUniverseDiscoveryConfig* config);

etcpal_error_t sacn_universe_discovery_create(const SacnUniverseDiscoveryConfig* config, sacn_universe_discovery_t* handle,
                                    SacnMcastInterface* netints, size_t num_netints);
etcpal_error_t sacn_universe_discovery_destroy(sacn_universe_discovery_t handle);

etcpal_error_t sacn_universe_discovery_(sacn_universe_discovery_t handle);

etcpal_error_t sacn_universe_discovery_reset_networking(sacn_universe_discovery_t handle, SacnMcastInterface* netints,
                                              size_t num_netints);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_UNIVERSE_DISCOVERY_H_ */
