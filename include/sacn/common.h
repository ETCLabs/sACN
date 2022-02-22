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

/**
 * @brief The maximum length of an sACN source name, including the null-terminator.
 *
 * E1.31 specifies that the Source Name field must be null-terminated on the wire.
 */
#define SACN_SOURCE_NAME_MAX_LEN 64

/**
 * The number of addresses in a DMX universe.
 */
#define DMX_ADDRESS_COUNT 512

/** A source discovered on an sACN network that has a CID - used by Receiver and Merge Receiver. */
typedef uint16_t sacn_remote_source_t;
/** An invalid remote source handle value. */
#define SACN_REMOTE_SOURCE_INVALID ((sacn_remote_source_t)-1)

/** The DMX start code. */
#define SACN_STARTCODE_DMX 0x00u
/** The per-address priority start code. */
#define SACN_STARTCODE_PRIORITY 0xddu

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
  /** An array of network interface IDs to which to restrict sACN traffic. The statuses are filled in for each
      interface. */
  SacnMcastInterface* netints;
  /** Size of netints array. */
  size_t num_netints;
} SacnNetintConfig;

etcpal_error_t sacn_init(const EtcPalLogParams* log_params, const SacnNetintConfig* sys_netint_config);
void sacn_deinit(void);

sacn_remote_source_t sacn_get_remote_source_handle(const EtcPalUuid* source_cid);
etcpal_error_t sacn_get_remote_source_cid(sacn_remote_source_t source_handle, EtcPalUuid* source_cid);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* SACN_COMMON_H_ */
