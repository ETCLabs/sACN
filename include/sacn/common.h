/******************************************************************************
 * Copyright 2024 ETC Inc.
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
 * @file sacn/common.h
 * @brief Common definitions for sACN
 */

#ifndef SACN_COMMON_H_
#define SACN_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "etcpal/uuid.h"

/**
 * @defgroup sACN sACN
 * @brief sACN: A Streaming ACN (sACN) implementation.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** A source discovered on an sACN network that has a CID - used by Receiver and Merge Receiver. */
typedef uint16_t sacn_remote_source_t;
/** An invalid remote source handle value. */
static const sacn_remote_source_t kSacnRemoteSourceInvalid = 0xFFFF;

enum
{
  /**
   * @brief The maximum length of an sACN source name, including the null-terminator.
   *
   * E1.31 specifies that the Source Name field must be null-terminated on the wire.
   */
  kSacnSourceNameMaxLen = 64,
  /** The number of addresses in a DMX universe. */
  kSacnDmxAddressCount = 512,
  /** The DMX start code. */
  kSacnStartcodeDmx = 0x00u,
  /** The per-address priority start code. */
  kSacnStartcodePriority = 0xddu,
  /** The lowest sACN universe number supported. */
  kSacnMinimumUniverse = 1,
  /** The highest sACN universe number supported. */
  kSacnMaximumUniverse = 63999
};

/**
 * This enum defines how the API module will use IPv4 and IPv6 networking.
 */
typedef enum
{
  /** Use IPv4 only. */
  kSacnIpV4Only,
  /** Use IPv6 only. */
  kSacnIpV6Only,
  /** Use both IPv4 and IPv6. */
  kSacnIpV4AndIpV6
} sacn_ip_support_t;

/**
 * On input, this structure is used to indicate a network interface to use.
 * On output, this structure indicates whether or not the operation was a success.
 */
typedef struct SacnMcastInterface
{
  /**
   * The multicast interface to use.
   */
  EtcPalMcastNetintId iface;

  /**
   * The status of the multicast interface. The interface is only usable if the status is #kEtcPalErrOk.
   */
  etcpal_error_t status;
} SacnMcastInterface;

/**
 * Network interface configuration information to give the sACN library. Multicast traffic will be restricted to the
 * network interfaces given. The statuses are filled in for each interface.
 */
typedef struct SacnNetintConfig
{
  /** An array of network interface IDs to which to restrict multicast traffic. The statuses are filled in for each
      interface. If this is null, and no_netints is false, all system interfaces will be used. */
  SacnMcastInterface* netints;
  /** Size of netints array. */
  size_t num_netints;
  /** If this is true, no network interfaces will be used for multicast. If any are specified in netints, they will be
      ignored and their statuses will be set to invalid. */
  bool no_netints;
} SacnNetintConfig;

/**
 * Initializes the members of a SacnNetintConfig to defaults.
 */
#define SACN_NETINT_CONFIG_DEFAULT_INIT {NULL, 0, false}

/** A mask of desired sACN features. See "sACN feature masks". */
typedef uint32_t sacn_features_t;

/**
 * @name sACN feature masks
 *
 * Pass one or more of these to sacn_init_features() to initialize the relevant sACN feature. Multiple features can be
 * requested using logical OR.
 *
 * Currently the DMX merger is the only feature that can be initialized individually. All other APIs can later be
 * initialized with SACN_FEATURES_ALL, which represents all APIs regardless if they have an individual feature defined
 * for them. Redundant initialization will ensure the same feature isn't initialized twice.
 *
 * @{
 */

#define SACN_FEATURE_DMX_MERGER ((sacn_features_t)(1u << 0)) /**< Use the sacn/dmx_merger module. */

// NOLINTNEXTLINE(cppcoreguidelines-macro-to-enum,modernize-macro-to-enum)
#define SACN_FEATURES_ALL 0xffffffffu /**< Use every available module. */

/**
 * @}
 */

etcpal_error_t sacn_init(const EtcPalLogParams* log_params, const SacnNetintConfig* sys_netint_config);
etcpal_error_t sacn_init_features(const EtcPalLogParams*  log_params,
                                  const SacnNetintConfig* sys_netint_config,
                                  sacn_features_t         features);
void           sacn_deinit(void);
void           sacn_deinit_features(sacn_features_t features);

sacn_remote_source_t sacn_get_remote_source_handle(const EtcPalUuid* source_cid);
etcpal_error_t       sacn_get_remote_source_cid(sacn_remote_source_t source_handle, EtcPalUuid* source_cid);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_COMMON_H_ */
