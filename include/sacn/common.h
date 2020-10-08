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
 * \file sacn/common.h
 * \brief Common definitions for sACN
 */

#ifndef SACN_COMMON_H_
#define SACN_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include "etcpal/error.h"
#include "etcpal/inet.h"
#include "etcpal/log.h"
#include "etcpal/uuid.h"

/*!
 * \defgroup sACN sACN
 * \brief sACN: A Streaming ACN (sACN) implementation.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief The maximum length of an sACN source name, including the null-terminator.
 *
 * E1.31 specifies that the Source Name field must be null-terminated on the wire.
 */
#define SACN_SOURCE_NAME_MAX_LEN 64

/*! The number of addresses in a DMX universe. */
#define DMX_ADDRESS_COUNT 512

/*! The data present in the header of an sACN data packet. */
typedef struct SacnHeaderData
{
  /*! The source's Component Identifier (CID). */
  EtcPalUuid cid;
  /*! A user-assigned name for displaying the identity of a source. */
  char source_name[SACN_SOURCE_NAME_MAX_LEN];
  /*! The sACN Universe identifier. Valid range is 1-63999, inclusive. */
  uint16_t universe_id;
  /*! The priority of the sACN data. Valid range is 0-200, inclusive. */
  uint8_t priority;
  /*! Whether the Preview_Data bit is set for the sACN data. From E1.31: "Indicates that the data in
   *  this packet is intended for use in visualization or media server preview applications and
   *  shall not be used to generate live output." */
  bool preview;
  /*! The start code of the DMX data. */
  uint8_t start_code;
  /*! The number of slots in the DMX data. */
  uint16_t slot_count;
} SacnHeaderData;

/*!
 * A set of identifying information for a network interface, for multicast purposes. When creating
 * network sockets to use with multicast sACN, the interface IP addresses don't matter and the
 * primary key for a network interface is simply a combination of the interface index and the IP
 * protocol used.
 */
// TODO CHRISTIAN : This is identical to the RdmnetMcastNetintId.  They need to be merged into ETCPal as
// EtcPalMcastNetintId.
typedef struct SacnMcastNetintId
{
  etcpal_iptype_t ip_type; /*!< The IP protocol used on the network interface. */
  unsigned int index;      /*!< The OS index of the network interface. */
} SacnMcastNetintId;

/*! The functions on the source and receiver that involve creation and networking change
 *   use this structure to indicate what interfaces were successfully created.
 * */
typedef struct SacnNetworkChangeResult
{
  /*! This array is owned by the application.  The library will fill in
   *   the array with the list of successfully added network interfaces (only up to
   *   the maximum size of the array).
   */
  SacnMcastNetintId* successful_interfaces;
  /*! On input, this is the maximum size of successful_interfaces.
   *   On output, this is the number of successful interfaces added by the library.  In the event that
   *   successful_interfaces is too small for the entire list, this count is bounded by the passed in max. */
  size_t successful_interfaces_count;
} SacnNetworkChangeResult;

etcpal_error_t sacn_init(const EtcPalLogParams* log_params);
void sacn_deinit(void);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* SACN_COMMON_H_ */
