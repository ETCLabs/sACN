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
 * \file sacn/source.h
 * \brief sACN Source API definitions
 *
 * Functions and definitions for the \ref sacn_source "sACN Source API" are contained in this
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

/*!
 * \defgroup sacn_source sACN Source
 * \ingroup sACN
 * \brief The sACN Source API.
 *
 * Components that send sACN are referred to as sACN Sources. Use this API to act as an sACN Source.
 *
 *  @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle to a sACN source. */
typedef int sacn_source_t;
/*! An invalid sACN source handle value. */
#define SACN_SOURCE_INVALID -1

/*!
 * \brief Constant for "infinite" when sending sACN universes.
 *
 * When using dynamic memory, this constant can be passed in when creating a source.
 * It represents an infinite number of universes that can be sent to.
 */
#define SACN_SOURCE_INFINITE_UNIVERSES 0

/*! A set of configuration information for a sACN source. */
typedef struct SacnSourceConfig
{
  /********* Required values **********/
  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];

  /********* Optional values **********/

  /*! The maximum number of sources this universe will send to.  May be #SACN_SOURCE_INFINITE_UNIVERSES.
      This parameter is ignored when configured to use static memory -- #SACN_SOURCE_MAX_UNIVERSES is used
     instead.*/
  size_t source_count_max;
  /*! (optional) array of network interfaces on which to listen to the specified universe. If NULL,
   *  all available network interfaces will be used. */
  const SacnMcastNetintId* netints;
  /*! Number of elements in the netints array. */
  size_t num_netints;
} SacnSourceConfig;

typedef struct SacnSourceUniverseConfig
{
  /********* Required values **********/
  uint16_t universe_id;
  uint8_t* slot_buffer;  //Values??
  size_t num_slots;

  /********* Optional values **********/
  uint8_t priority;
  uint8_t* priority_buffer;  //Priority buffer will always be checked first., then priority, then slots processed
  size_t num_priorities;  //???

  /*! A set of option flags. See "sACN receiver flags". */
  unsigned int flags; //PREVIEW??

} SacnSourceUniverseConfig;

void sacn_source_config_set_defaults(SacnSourceConfig* config);
void sacn_source_universe_config_set_defaults(SacnSourceUniverseConfig* config);

etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle);
etcpal_error_t sacn_source_destroy(sacn_source_t handle);

sacn_source_add_universe; //Starts universe discovery, starts data/priority/per-address
sacn_source_remove_universe;  //Gracefully shutds down universe (sending values properly), stops universe discovery.

sacn_source_send_non_dmx;  //alternate start code stuff


//Instead of adding start codes, have a data/priority section for a handle, and allow sending of other start codes manually?
etcpal_error_t sacn_source_add_start_code(sacn_source_t handle, const SacnStartCodeConfig* sc_config);
etcpal_error_t sacn_source_remove_start_code(sacn_source_t handle, uint8_t start_code);

//NOT SURE WE NEED THESE, or changing priority/per-addresssssss.. Maybe so..
etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint8_t new_priority);
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, bool new_preview_flag);
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name);

void sacn_source_set_dirty(sacn_source_t handle, uint8_t start_code);
void sacn_sources_set_dirty(const SacnSourceStartCodePair* start_codes, size_t num_start_codes);
void sacn_source_send_now(sacn_source_t handle, uint8_t start_code);
void sacn_sources_send_now(const SacnSourceStartCodePair* start_codes, size_t num_start_codes);

etcpal_error_t sacn_source_reset_networking(sacn_receiver_t handle, const SacnMcastNetintId* netints,
                                              size_t num_netints);
//What about multiple sources for same universe??

size_t sacn_process_sources(void);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* SACN_SOURCE_H_ */
