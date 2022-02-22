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

#include "sacn/private/common.h"

#include "sacn/private/mem.h"
#include "sacn/private/source_loss.h"
#include "sacn/private/receiver_state.h"
#include "sacn/private/source_state.h"
#include "sacn/private/sockets.h"
#include "sacn/private/source.h"
#include "sacn/private/receiver.h"
#include "sacn/private/dmx_merger.h"
#include "sacn/private/merge_receiver.h"
#include "sacn/private/source_detector.h"

/*************************** Private constants *******************************/

#define SACN_ETCPAL_FEATURES \
  (ETCPAL_FEATURE_SOCKETS | ETCPAL_FEATURE_TIMERS | ETCPAL_FEATURE_NETINTS | ETCPAL_FEATURE_LOGGING)

/***************************** Global variables ******************************/

const EtcPalLogParams* sacn_log_params;

/**************************** Private variables ******************************/

static struct SacnState
{
  bool initted;
  EtcPalLogParams log_params;
} sacn_pool_sacn_state;

static etcpal_mutex_t sacn_mutex;

/*************************** Function definitions ****************************/

/**
 * @brief Initialize the sACN library.
 *
 * Do all necessary initialization before other sACN API functions can be called.
 *
 * @param[in] log_params A struct used by the library to log messages, or NULL for no logging. If
 *                       #SACN_LOGGING_ENABLED is 0, this parameter is ignored.
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the library will be
 * limited to, and the status codes are filled in.  If NULL, the library is allowed to use all available system
 * interfaces.
 * @return #kEtcPalErrOk: Initialization successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_init(const EtcPalLogParams* log_params, const SacnNetintConfig* sys_netint_config)
{
  etcpal_error_t res = kEtcPalErrAlready;

  if (!sacn_pool_sacn_state.initted)
  {
    res = kEtcPalErrOk;

    // Init the log params early so the other modules can log things on initialization
    if (log_params)
    {
      sacn_pool_sacn_state.log_params = *log_params;
      sacn_log_params = &sacn_pool_sacn_state.log_params;
    }

    bool etcpal_initted = false;
    if (res == kEtcPalErrOk)
      etcpal_initted = ((res = etcpal_init(SACN_ETCPAL_FEATURES)) == kEtcPalErrOk);

    bool mutex_initted = false;
    if (res == kEtcPalErrOk)
    {
      mutex_initted = etcpal_mutex_create(&sacn_mutex);
      if (!mutex_initted)
        res = kEtcPalErrSys;
    }

#if SACN_RECEIVER_ENABLED
    bool receiver_mem_initted = false;
    if (res == kEtcPalErrOk)
      receiver_mem_initted = ((res = sacn_receiver_mem_init(SACN_RECEIVER_MAX_THREADS)) == kEtcPalErrOk);
#endif  // SACN_RECEIVER_ENABLED

#if SACN_SOURCE_ENABLED
    bool source_mem_initted = false;
    if (res == kEtcPalErrOk)
      source_mem_initted = ((res = sacn_source_mem_init()) == kEtcPalErrOk);
#endif  // SACN_SOURCE_ENABLED

#if SACN_MERGE_RECEIVER_ENABLED
    bool merge_receiver_mem_initted = false;
    if (res == kEtcPalErrOk)
      merge_receiver_mem_initted = ((res = sacn_merge_receiver_mem_init(SACN_RECEIVER_MAX_THREADS)) == kEtcPalErrOk);
#endif  // SACN_MERGE_RECEIVER_ENABLED

#if SACN_SOURCE_DETECTOR_ENABLED
    bool source_detector_mem_initted = false;
    if (res == kEtcPalErrOk)
      source_detector_mem_initted = ((res = sacn_source_detector_mem_init()) == kEtcPalErrOk);
#endif  // SACN_SOURCE_DETECTOR_ENABLED

    bool sockets_initted = false;
    if (res == kEtcPalErrOk)
      sockets_initted = ((res = sacn_sockets_init(sys_netint_config)) == kEtcPalErrOk);

#if SACN_RECEIVER_ENABLED
    bool source_loss_initted = false;
    bool receiver_state_initted = false;
    bool receiver_initted = false;
    if (res == kEtcPalErrOk)
      source_loss_initted = ((res = sacn_source_loss_init()) == kEtcPalErrOk);
    if (res == kEtcPalErrOk)
      receiver_state_initted = ((res = sacn_receiver_state_init()) == kEtcPalErrOk);
    if (res == kEtcPalErrOk)
      receiver_initted = ((res = sacn_receiver_init()) == kEtcPalErrOk);
#endif  // SACN_RECEIVER_ENABLED

#if SACN_SOURCE_ENABLED
    bool source_state_initted = false;
    bool source_initted = false;
    if (res == kEtcPalErrOk)
      source_state_initted = ((res = sacn_source_state_init()) == kEtcPalErrOk);
    if (res == kEtcPalErrOk)
      source_initted = ((res = sacn_source_init()) == kEtcPalErrOk);
#endif  // SACN_SOURCE_ENABLED

#if SACN_DMX_MERGER_ENABLED
    bool merger_initted = false;
    if (res == kEtcPalErrOk)
      merger_initted = ((res = sacn_dmx_merger_init()) == kEtcPalErrOk);
#endif  // SACN_DMX_MERGER_ENABLED

#if SACN_MERGE_RECEIVER_ENABLED
    bool merge_receiver_initted = false;
    if (res == kEtcPalErrOk)
      merge_receiver_initted = ((res = sacn_merge_receiver_init()) == kEtcPalErrOk);
#endif  // SACN_MERGE_RECEIVER_ENABLED

#if SACN_SOURCE_DETECTOR_ENABLED
    bool source_detector_initted = false;
    if (res == kEtcPalErrOk)
      source_detector_initted = ((res = sacn_source_detector_init()) == kEtcPalErrOk);
#endif  // SACN_SOURCE_DETECTOR_ENABLED

    if (res == kEtcPalErrOk)
    {
      sacn_pool_sacn_state.initted = true;
    }
    else
    {
      // Clean up
#if SACN_SOURCE_DETECTOR_ENABLED
      if (source_detector_initted)
        sacn_source_detector_deinit();
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
      if (merge_receiver_initted)
        sacn_merge_receiver_deinit();
#endif  // SACN_MERGE_RECEIVER_ENABLED
#if SACN_DMX_MERGER_ENABLED
      if (merger_initted)
        sacn_dmx_merger_deinit();
#endif  // SACN_DMX_MERGER_ENABLED
#if SACN_SOURCE_ENABLED
      if (source_initted)
        sacn_source_deinit();
      if (source_state_initted)
        sacn_source_state_deinit();
#endif  // SACN_SOURCE_ENABLED
#if SACN_RECEIVER_ENABLED
      if (receiver_initted)
        sacn_receiver_deinit();
      if (receiver_state_initted)
        sacn_receiver_state_deinit();
      if (source_loss_initted)
        sacn_source_loss_deinit();
#endif  // SACN_RECEIVER_ENABLED
      if (sockets_initted)
        sacn_sockets_deinit();
#if SACN_SOURCE_DETECTOR_ENABLED
      if (source_detector_mem_initted)
        sacn_source_detector_mem_deinit();
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
      if (merge_receiver_mem_initted)
        sacn_merge_receiver_mem_deinit();
#endif  // SACN_MERGE_RECEIVER_ENABLED
#if SACN_SOURCE_ENABLED
      if (source_mem_initted)
        sacn_source_mem_deinit();
#endif  // SACN_SOURCE_ENABLED
#if SACN_RECEIVER_ENABLED
      if (receiver_mem_initted)
        sacn_receiver_mem_deinit();
#endif  // SACN_RECEIVER_ENABLED
      if (mutex_initted)
        etcpal_mutex_destroy(&sacn_mutex);
      if (etcpal_initted)
        etcpal_deinit(SACN_ETCPAL_FEATURES);

      sacn_log_params = NULL;
    }
  }

  return res;
}

/**
 * @brief Deinitialize the sACN library.
 *
 * Set the sACN library back to an uninitialized state. Calls to other sACN API functions will fail
 * until sacn_init() is called again.
 *
 * This function is not thread safe with respect to other sACN API functions. Make sure to join your threads that use
 * the APIs before calling this.
 */
void sacn_deinit(void)
{
  if (sacn_pool_sacn_state.initted)
  {
    sacn_pool_sacn_state.initted = false;

#if SACN_SOURCE_DETECTOR_ENABLED
    sacn_source_detector_deinit();
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
    sacn_merge_receiver_deinit();
#endif  // SACN_MERGE_RECEIVER_ENABLED
#if SACN_DMX_MERGER_ENABLED
    sacn_dmx_merger_deinit();
#endif  // SACN_DMX_MERGER_ENABLED
#if SACN_SOURCE_ENABLED
    sacn_source_deinit();
    sacn_source_state_deinit();
#endif  // SACN_SOURCE_ENABLED
#if SACN_RECEIVER_ENABLED
    sacn_receiver_deinit();
    sacn_receiver_state_deinit();
    sacn_source_loss_deinit();
#endif  // SACN_RECEIVER_ENABLED
    sacn_sockets_deinit();
#if SACN_SOURCE_DETECTOR_ENABLED
    sacn_source_detector_mem_deinit();
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
    sacn_merge_receiver_mem_deinit();
#endif  // SACN_MERGE_RECEIVER_ENABLED
#if SACN_SOURCE_ENABLED
    sacn_source_mem_deinit();
#endif  // SACN_SOURCE_ENABLED
#if SACN_RECEIVER_ENABLED
    sacn_receiver_mem_deinit();
#endif  // SACN_RECEIVER_ENABLED
    etcpal_mutex_destroy(&sacn_mutex);
    etcpal_deinit(SACN_ETCPAL_FEATURES);

    sacn_log_params = NULL;
  }
}

#if SACN_RECEIVER_ENABLED || DOXYGEN

/**
 * @brief Converts a remote source CID to the corresponding handle, or #SACN_REMOTE_SOURCE_INVALID if not found.
 *
 * This is a simple conversion from a remote source CID to it's corresponding remote source handle. A handle will be
 * returned only if it is a source that has been discovered by a receiver, merge receiver, or source detector.
 *
 * @param[in] source_cid The UUID of the remote source CID.
 * @return The remote source handle, or #SACN_REMOTE_SOURCE_INVALID if not found.
 */
sacn_remote_source_t sacn_get_remote_source_handle(const EtcPalUuid* source_cid)
{
  sacn_remote_source_t result = SACN_REMOTE_SOURCE_INVALID;

  if (sacn_lock())
  {
    result = get_remote_source_handle(source_cid);
    sacn_unlock();
  }

  return result;
}

/**
 * @brief Converts a remote source handle to the corresponding source CID.
 *
 * @param[in] source_handle The handle of the remote source.
 * @param[out] source_cid The UUID of the source CID. Only written to if #kEtcPalErrOk is returned.
 *
 * @return #kEtcPalErrOk: Lookup was successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotFound: The source handle does not match a source that was found by a receiver, merge receiver,
 * or source detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_get_remote_source_cid(sacn_remote_source_t source_handle, EtcPalUuid* source_cid)
{
  etcpal_error_t result = kEtcPalErrOk;

  if ((source_handle == SACN_REMOTE_SOURCE_INVALID) || !source_cid)
  {
    result = kEtcPalErrInvalid;
  }
  else
  {
    if (sacn_lock())
    {
      const EtcPalUuid* cid = get_remote_source_cid(source_handle);

      if (cid)
        *source_cid = *cid;
      else
        result = kEtcPalErrNotFound;

      sacn_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

#endif  // SACN_RECEIVER_ENABLED

bool sacn_lock(void)
{
  return etcpal_mutex_lock(&sacn_mutex);
}

void sacn_unlock(void)
{
  etcpal_mutex_unlock(&sacn_mutex);
}

bool sacn_initialized(void)
{
  return sacn_pool_sacn_state.initted;
}
