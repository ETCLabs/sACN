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

typedef struct SacnStartCodeConfig
{
  uint8_t start_code;
  bool always_send;
  uint8_t* slot_buffer;
  size_t num_slots;
} SacnStartCodeConfig;

typedef struct SacnSourceConfig
{
  /* Required configuration data */
  EtcPalUuid cid;
  uint16_t universe_id;

  /* Optional configuration data */
  uint8_t priority;
  const EtcPalMcastNetintId* netints;
  size_t num_netints;
  bool preview;
  char name[SACN_SOURCE_NAME_MAX_LEN];
} SacnSourceConfig;

typedef struct SacnSourceStartCodePair
{
  sacn_source_t handle;
  uint8_t start_code;
} SacnSourceStartCodePair;

void sacn_source_config_set_defaults(SacnSourceConfig* config);

etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle);
etcpal_error_t sacn_source_destroy(sacn_source_t handle);

etcpal_error_t sacn_source_add_start_code(sacn_source_t handle, const SacnStartCodeConfig* sc_config);
etcpal_error_t sacn_source_remove_start_code(sacn_source_t handle, uint8_t start_code);

etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint8_t new_priority);
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, bool new_preview_flag);
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name);

void sacn_source_set_dirty(sacn_source_t handle, uint8_t start_code);
void sacn_sources_set_dirty(const SacnSourceStartCodePair* start_codes, size_t num_start_codes);
void sacn_source_send_now(sacn_source_t handle, uint8_t start_code);
void sacn_sources_send_now(const SacnSourceStartCodePair* start_codes, size_t num_start_codes);

size_t sacn_process_sources(void);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* SACN_SOURCE_H_ */
