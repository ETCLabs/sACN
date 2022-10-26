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
 * @file sacn/source_detector.h
 * @brief sACN Source Detector API definitions
 *
 * Functions and definitions for the @ref sacn_source_detector "sACN Source Detector API" are contained in this
 * header.
 */

#ifndef SACN_SOURCE_DETECTOR_H_
#define SACN_SOURCE_DETECTOR_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "etcpal/netint.h"
#include "etcpal/uuid.h"
#include "sacn/common.h"

/**
 * @defgroup sacn_source_detector sACN Source Detector
 * @ingroup sACN
 * @brief The sACN Source Detector API
 *
 * sACN sources often periodically send Universe Discovery packets to announce what universes they are sourcing.
 * Use this API to monitor such traffic for your own needs.
 *
 * There can only ever be one instance of the source detector, but that instance still needs to be created and can be
 * destroyed.
 *
 * Usage:
 * @code
 * #include "sacn/source_detector.h"
 *
 * EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
 * // Initialize log_params...
 *
 * etcpal_error_t init_result = sacn_init(&log_params, NULL);
 * // Or, to init without worrying about logs from the sACN library...
 * etcpal_error_t init_result = sacn_init(NULL, NULL);
 *
 * SacnSourceDetectorConfig my_config = SACN_SOURCE_DETECTOR_CONFIG_DEFAULT_INIT;
 * my_config.callbacks.source_updated = my_source_updated;
 * my_config.callbacks.source_expired = my_source_expired;
 * my_config.callbacks.limit_exceeded = my_limit_exceeded;
 *
 * SacnMcastInterface my_netints[NUM_MY_NETINTS];
 * // Assuming my_netints and NUM_MY_NETINTS are initialized by the application...
 *
 * SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
 * netint_config.netints = my_netints;
 * netint_config.num_netints = NUM_MY_NETINTS;
 *
 * // If you want to specify specific network interfaces to use:
 * etcpal_error_t create_result = sacn_source_detector_create(&my_config, &netint_config);
 * // Or, if you just want to use all network interfaces:
 * etcpal_error_t create_result = sacn_source_detector_create(&my_config, NULL);
 * // Check create_result here...
 *
 * // Now the thread is running and your callbacks will handle application-side processing.
 *
 * // What if your network interfaces change? Update my_netints and call this:
 * etcpal_error_t reset_result = sacn_source_detector_reset_networking(my_netints, NUM_MY_NETINTS);
 * // Check reset_result here...
 *
 * // To destroy the source detector, call this:
 * sacn_source_detector_destroy();
 *
 * // During application shutdown, everything can be cleaned up by calling sacn_deinit.
 * sacn_deinit();
 * @endcode
 *
 * Callback demonstrations:
 * @code
 * void my_source_updated(sacn_remote_source_t handle, const EtcPalUuid* cid, const char* name,
 *                        const uint16_t* sourced_universes, size_t num_sourced_universes, void* context)
 * {
 *   if (cid && name)
 *   {
 *     char cid_str[ETCPAL_UUID_STRING_BYTES];
 *     etcpal_uuid_to_string(cid, cid_str);
 *     printf("Source Detector: Source %s (name %s) ", cid_str, name);
 *     if(sourced_universes)
 *     {
 *       printf("is active on these universes: ");
 *       for(size_t i = 0; i < num_sourced_universes; ++i)
 *         printf("%d ", sourced_universes[i]);
 *       printf("\n");
 *     }
 *     else
 *     {
 *       printf("is not active on any universes.\n");
 *     }
 *   }
 * }
 *
 * void my_source_expired(sacn_remote_source_t handle, const EtcPalUuid* cid, const char* name, void* context)
 * {
 *   if (cid && name)
 *   {
 *     char cid_str[ETCPAL_UUID_STRING_BYTES];
 *     etcpal_uuid_to_string(cid, cid_str);
 *     printf("Source Detector: Source %s (name %s) has expired.\n", cid_str, name);
 *   }
 * }
 *
 * void my_limit_exceeded(void* context)
 * {
 *   printf("Source Detector: Source/universe limit exceeded!\n");
 * }
 * @endcode
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Constant for "infinite" when listening for sources or universes on a source.
 *
 * When using dynamic memory, this constant can be passed in when creating a source detector.
 * It represents an infinite number of sources or universes on a source.
 */
#define SACN_SOURCE_DETECTOR_INFINITE 0

/**
 * @brief Notify that a source is new or has changed.
 *
 * This passes the source's current universe list, but you will only get this callback when the module detects
 * that the source is new or the list has somehow changed.
 *
 * The protocol requires the list of sourced universes to be numerically sorted. The library enforces this rule by
 * checking that the universe list is in ascending order before notifying.
 *
 * @param[in] handle The handle uniquely identifying the source.
 * @param[in] cid The CID of the source.
 * @param[in] name The null-terminated UTF-8 string.
 * @param[in] sourced_universes Numerically sorted array of the currently sourced universes.  Will be NULL if the source
 * is not currently transmitting any universes.
 * @param[in] num_sourced_universes Size of the sourced_universes array.  Will be 0 if the source is not currently
 * transmitting any universes.
 * @param[in] context Context pointer that was given at the creation of the source detector instance.
 */
typedef void (*SacnSourceDetectorSourceUpdatedCallback)(sacn_remote_source_t handle, const EtcPalUuid* cid,
                                                        const char* name, const uint16_t* sourced_universes,
                                                        size_t num_sourced_universes, void* context);

/**
 * @brief Notify that a source is no longer transmitting Universe Discovery messages.
 *
 * @param[in] handle The handle uniquely identifying the source.
 * @param[in] cid The CID of the source.
 * @param[in] name The null-terminated UTF-8 string.
 * @param[in] context Context pointer that was given at the creation of the source detector instance.
 */
typedef void (*SacnSourceDetectorSourceExpiredCallback)(sacn_remote_source_t handle, const EtcPalUuid* cid,
                                                        const char* name, void* context);

/**
 * @brief Notify that the module has run out of memory to track universes or sources.
 *
 * If #SACN_DYNAMIC_MEM was defined to 1 when sACN was compiled (the default on non-embedded
 * platforms), and the configuration you pass to sacn_source_detector_create() has source_count_max and
 * universes_per_source_max set to #SACN_SOURCE_DETECTOR_INFINITE, this callback will never be called (except for the
 * rare case where a heap allocation function fails) and may be set to NULL.
 *
 * If #SACN_DYNAMIC_MEM was defined to 0 when sACN was compiled, source_count_max and universes_per_source_max are
 * ignored and #SACN_SOURCE_DETECTOR_MAX_SOURCES and #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE are used instead.
 *
 * This callback is rate-limited: it will only be called the first time a source or universe limit is exceeded.  After
 * that, it will not be called until the number of sources or universes has dropped below their limit and hits it again.
 *
 * @param[in] context Context pointer that was given at the creation of the source detector instance.
 */
typedef void (*SacnSourceDetectorLimitExceededCallback)(void* context);

/** A set of callback functions that the library uses to notify the application about source detector events. */
typedef struct SacnSourceDetectorCallbacks
{
  SacnSourceDetectorSourceUpdatedCallback source_updated; /**< Required */
  SacnSourceDetectorSourceExpiredCallback source_expired; /**< Required */
  SacnSourceDetectorLimitExceededCallback limit_exceeded; /**< Optional */
  void* context; /**< (optional) Pointer to opaque data passed back with each callback. */
} SacnSourceDetectorCallbacks;

/** A set of configuration information for the sACN Source Detector. */
typedef struct SacnSourceDetectorConfig
{
  /** The callbacks the detector will use to notify the application of events. */
  SacnSourceDetectorCallbacks callbacks;

  /********* Optional values **********/

  /** The maximum number of sources the detector will record.  It is recommended that applications using dynamic
     memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when configured to use
     static memory -- #SACN_SOURCE_DETECTOR_MAX_SOURCES is used instead.*/
  int source_count_max;

  /** The maximum number of universes the detector will record for a source.  It is recommended that applications using
     dynamic memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when configured to
     use static memory -- #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE is used instead.*/
  int universes_per_source_max;

  /** What IP networking the source detector will support.  The default is #kSacnIpV4AndIpV6. */
  sacn_ip_support_t ip_supported;
} SacnSourceDetectorConfig;

/** A default-value initializer for an SacnSourceDetectorConfig struct. */
#define SACN_SOURCE_DETECTOR_CONFIG_DEFAULT_INIT                                                             \
  {                                                                                                          \
    {NULL, NULL, NULL, NULL}, SACN_SOURCE_DETECTOR_INFINITE, SACN_SOURCE_DETECTOR_INFINITE, kSacnIpV4AndIpV6 \
  }

void sacn_source_detector_config_init(SacnSourceDetectorConfig* config);

etcpal_error_t sacn_source_detector_create(const SacnSourceDetectorConfig* config,
                                           const SacnNetintConfig* netint_config);
void sacn_source_detector_destroy();

etcpal_error_t sacn_source_detector_reset_networking(const SacnNetintConfig* sys_netint_config);

size_t sacn_source_detector_get_network_interfaces(EtcPalMcastNetintId* netints, size_t netints_size);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_SOURCE_DETECTOR_H_ */
