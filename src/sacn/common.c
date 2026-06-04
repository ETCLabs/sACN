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
#include "sacn/private/source_detector_state.h"

/***************************** Global variables ******************************/

const EtcPalLogParams* sacn_log_params;

/**************************** Private variables ******************************/

/**
 * State to track library initialization and logging parameters.
 */
static struct SacnState
{
  /** Initialization counter for the DMX merger feature. */
  int dmx_merger_feature_init_count;
  /** Initialization counter for all network features. */
  int all_network_features_init_count;
  /** The current log parameters being used by the library. */
  EtcPalLogParams log_params;
} sacn_pool_sacn_state;

static etcpal_mutex_t sacn_receiver_sockets_mutex;
static etcpal_mutex_t sacn_receiver_state_mutex;
static etcpal_mutex_t sacn_source_mutex;

/*************************** Function definitions ****************************/

/**
 * @brief Initialize all features of the sACN library.
 *
 * Do all necessary initialization before other sACN API functions can be called.
 *
 * Redundant initialization is permitted - the library tracks counters for each feature and expects deinit to be called
 * the same number of times as init for each feature.
 *
 * @param[in] log_params A struct used by the library to log messages, or NULL for no logging. If
 *                       #SACN_LOGGING_ENABLED is 0, this parameter is ignored.
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the library will be
 * limited to (with the added option of not allowing any interfaces to be used), and the status codes are filled in.  If
 * NULL, the library is allowed to use all available system interfaces.
 * @return #kEtcPalErrOk: Initialization successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_init(const EtcPalLogParams* log_params, const SacnNetintConfig* sys_netint_config)
{
  return sacn_init_features(log_params, sys_netint_config, SACN_FEATURES_ALL);
}

/**
 * @brief Initialize all features of the sACN library.
 *
 * Do all necessary initialization before other sACN API functions can be called.
 *
 * Redundant initialization is permitted - the library tracks counters for each feature and expects deinit to be called
 * the same number of times as init for each feature.
 *
 * @param[in] log_params A struct used by the library to log messages, or NULL for no logging. If
 *                       #SACN_LOGGING_ENABLED is 0, this parameter is ignored.
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the library will be
 * limited to (with the added option of not allowing any interfaces to be used), and the status codes are filled in.  If
 * NULL, the library is allowed to use all available system interfaces.
 * @param[in] callbacks Optional. A struct of callback functions for the library to use to notify the application of
 * events.
 * @return #kEtcPalErrOk: Initialization successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_init_with_cb(const EtcPalLogParams*     log_params,
                                 const SacnNetintConfig*    sys_netint_config,
                                 const SacnCommonCallbacks* callbacks)
{
  return sacn_init_features_with_cb(log_params, sys_netint_config, SACN_FEATURES_ALL, callbacks);
}

/**
 * @brief Initialize specific features of the sACN library.
 *
 * Do all necessary initialization before other sACN API functions can be called.
 *
 * Redundant initialization of features is permitted - the library tracks counters for each feature and expects deinit
 * to be called the same number of times as init for each feature.
 *
 * @param[in] log_params A struct used by the library to log messages, or NULL for no logging. If
 *                       #SACN_LOGGING_ENABLED is 0, this parameter is ignored.
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the library will be
 * limited to (with the added option of not allowing any interfaces to be used), and the status codes are filled in.  If
 * NULL, the library is allowed to use all available system interfaces.
 * @param[in] features Mask of sACN features to initialize.
 * @return #kEtcPalErrOk: Initialization successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_init_features(const EtcPalLogParams*  log_params,
                                  const SacnNetintConfig* sys_netint_config,
                                  sacn_features_t         features)
{
  return sacn_init_features_with_cb(log_params, sys_netint_config, features, NULL);
}

// NOLINTBEGIN(readability-function-cognitive-complexity)

/**
 * @brief Initialize specific features of the sACN library.
 *
 * Do all necessary initialization before other sACN API functions can be called.
 *
 * Redundant initialization of features is permitted - the library tracks counters for each feature and expects deinit
 * to be called the same number of times as init for each feature.
 *
 * @param[in] log_params A struct used by the library to log messages, or NULL for no logging. If
 *                       #SACN_LOGGING_ENABLED is 0, this parameter is ignored.
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the library will be
 * limited to (with the added option of not allowing any interfaces to be used), and the status codes are filled in.  If
 * NULL, the library is allowed to use all available system interfaces.
 * @param[in] features Mask of sACN features to initialize.
 * @param[in] callbacks Optional. A struct of callback functions for the library to use to notify the application of
 * events.
 * @return #kEtcPalErrOk: Initialization successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_init_features_with_cb(const EtcPalLogParams*     log_params,
                                          const SacnNetintConfig*    sys_netint_config,
                                          sacn_features_t            features,
                                          const SacnCommonCallbacks* callbacks)
{
  sacn_features_t features_to_init = features;

  // Avoid redundant init
  if ((features_to_init & SACN_FEATURE_DMX_MERGER) != 0)
  {
    if (sacn_pool_sacn_state.dmx_merger_feature_init_count > 0)
      features_to_init = (features_to_init & ~SACN_FEATURE_DMX_MERGER);
  }

  if ((features_to_init & SACN_ALL_NETWORK_FEATURES) == SACN_ALL_NETWORK_FEATURES)
  {
    if (sacn_pool_sacn_state.all_network_features_init_count > 0)
      features_to_init = (features_to_init & ~SACN_ALL_NETWORK_FEATURES);
  }

  bool log_params_initted             = false;
  bool etcpal_logging_initted         = false;
  bool etcpal_sockets_initted         = false;
  bool etcpal_timers_initted          = false;
  bool etcpal_netints_initted         = false;
  bool receiver_sockets_mutex_initted = false;
  bool receiver_state_mutex_initted   = false;
  bool source_mutex_initted           = false;
#if SACN_RECEIVER_ENABLED
  bool receiver_mem_initted = false;
#endif  // SACN_RECEIVER_ENABLED
#if SACN_SOURCE_ENABLED
  bool source_mem_initted = false;
#endif  // SACN_SOURCE_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
  bool merge_receiver_mem_initted = false;
#endif  // SACN_MERGE_RECEIVER_ENABLED
#if SACN_SOURCE_DETECTOR_ENABLED
  bool source_detector_mem_initted = false;
#endif  // SACN_SOURCE_DETECTOR_ENABLED
  bool sockets_initted = false;
#if SACN_MERGE_RECEIVER_ENABLED
  bool merge_receiver_initted = false;
#endif  // SACN_MERGE_RECEIVER_ENABLED
#if SACN_SOURCE_DETECTOR_ENABLED
  bool source_detector_state_initted = false;
  bool source_detector_initted       = false;
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_RECEIVER_ENABLED
  bool source_loss_initted    = false;
  bool receiver_state_initted = false;
  bool receiver_initted       = false;
#endif  // SACN_RECEIVER_ENABLED
#if SACN_SOURCE_ENABLED
  bool source_state_initted = false;
  bool source_initted       = false;
#endif  // SACN_SOURCE_ENABLED
#if SACN_DMX_MERGER_ENABLED
  bool merger_initted = false;
#endif  // SACN_DMX_MERGER_ENABLED

  // Init the log params early so the other modules can log things on initialization
  if (log_params && !sacn_log_params)
  {
    sacn_pool_sacn_state.log_params = *log_params;
    sacn_log_params                 = &sacn_pool_sacn_state.log_params;
    log_params_initted              = true;
  }

  etcpal_error_t res = kEtcPalErrOk;
  if ((features_to_init & SACN_ALL_NETWORK_FEATURES) == SACN_ALL_NETWORK_FEATURES)
  {
    if (res == kEtcPalErrOk)
      etcpal_logging_initted = ((res = etcpal_init(ETCPAL_FEATURE_LOGGING)) == kEtcPalErrOk);

    if (res == kEtcPalErrOk)
    {
      etcpal_sockets_initted = ((res = etcpal_init(ETCPAL_FEATURE_SOCKETS)) == kEtcPalErrOk);
      if (!etcpal_sockets_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE ETCPAL SOCKETS!");
    }

    if (res == kEtcPalErrOk)
    {
      etcpal_timers_initted = ((res = etcpal_init(ETCPAL_FEATURE_TIMERS)) == kEtcPalErrOk);
      if (!etcpal_sockets_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE ETCPAL TIMERS!");
    }

    if (res == kEtcPalErrOk)
    {
      etcpal_netints_initted = ((res = etcpal_init(ETCPAL_FEATURE_NETINTS)) == kEtcPalErrOk);
      if (!etcpal_sockets_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE ETCPAL NETINTS!");
    }

    if (res == kEtcPalErrOk)
    {
      receiver_sockets_mutex_initted = etcpal_mutex_create(&sacn_receiver_sockets_mutex);
      if (!receiver_sockets_mutex_initted)
      {
        res = kEtcPalErrSys;
        SACN_LOG_CRIT("FAILED TO INITIALIZE RECEIVER SOCKETS MUTEX!");
      }
    }

    if (res == kEtcPalErrOk)
    {
      receiver_state_mutex_initted = etcpal_mutex_create(&sacn_receiver_state_mutex);
      if (!receiver_state_mutex_initted)
      {
        res = kEtcPalErrSys;
        SACN_LOG_CRIT("FAILED TO INITIALIZE RECEIVER STATE MUTEX!");
      }
    }

    if (res == kEtcPalErrOk)
    {
      source_mutex_initted = etcpal_mutex_create(&sacn_source_mutex);
      if (!source_mutex_initted)
      {
        res = kEtcPalErrSys;
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE MUTEX!");
      }
    }

#if SACN_RECEIVER_ENABLED
    if (res == kEtcPalErrOk)
    {
      receiver_mem_initted = ((res = sacn_receiver_mem_init(SACN_RECEIVER_MAX_THREADS)) == kEtcPalErrOk);
      if (!receiver_mem_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE RECEIVER MEMORY!");
    }
#endif  // SACN_RECEIVER_ENABLED

#if SACN_SOURCE_ENABLED
    if (res == kEtcPalErrOk)
    {
      source_mem_initted = ((res = sacn_source_mem_init()) == kEtcPalErrOk);
      if (!source_mem_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE MEMORY!");
    }
#endif  // SACN_SOURCE_ENABLED

#if SACN_MERGE_RECEIVER_ENABLED
    if (res == kEtcPalErrOk)
    {
      merge_receiver_mem_initted = ((res = sacn_merge_receiver_mem_init(SACN_RECEIVER_MAX_THREADS)) == kEtcPalErrOk);
      if (!merge_receiver_mem_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE MERGE RECEIVER MEMORY!");
    }
#endif  // SACN_MERGE_RECEIVER_ENABLED

#if SACN_SOURCE_DETECTOR_ENABLED
    if (res == kEtcPalErrOk)
    {
      source_detector_mem_initted = ((res = sacn_source_detector_mem_init()) == kEtcPalErrOk);
      if (!source_detector_mem_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE DETECTOR MEMORY!");
    }
#endif  // SACN_SOURCE_DETECTOR_ENABLED

    if (res == kEtcPalErrOk)
    {
      sockets_initted = ((res = sacn_sockets_init(sys_netint_config, callbacks)) == kEtcPalErrOk);
      if (!sockets_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOCKETS!");
    }

#if SACN_MERGE_RECEIVER_ENABLED
    if (res == kEtcPalErrOk)
    {
      merge_receiver_initted = ((res = sacn_merge_receiver_init()) == kEtcPalErrOk);
      if (!merge_receiver_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE MERGE RECEIVER!");
    }
#endif  // SACN_MERGE_RECEIVER_ENABLED

#if SACN_SOURCE_DETECTOR_ENABLED
    if (res == kEtcPalErrOk)
    {
      source_detector_state_initted = ((res = sacn_source_detector_state_init()) == kEtcPalErrOk);
      if (!source_detector_state_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE DETECTOR STATE!");
    }

    if (res == kEtcPalErrOk)
    {
      source_detector_initted = ((res = sacn_source_detector_init()) == kEtcPalErrOk);
      if (!source_detector_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE DETECTOR!");
    }
#endif  // SACN_SOURCE_DETECTOR_ENABLED

#if SACN_RECEIVER_ENABLED
    if (res == kEtcPalErrOk)
    {
      source_loss_initted = ((res = sacn_source_loss_init()) == kEtcPalErrOk);
      if (!source_loss_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE LOSS!");
    }

    if (res == kEtcPalErrOk)
    {
      receiver_state_initted = ((res = sacn_receiver_state_init()) == kEtcPalErrOk);
      if (!receiver_state_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE RECEIVER STATE!");
    }

    if (res == kEtcPalErrOk)
    {
      receiver_initted = ((res = sacn_receiver_init()) == kEtcPalErrOk);
      if (!receiver_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE RECEIVER!");
    }
#endif  // SACN_RECEIVER_ENABLED

#if SACN_SOURCE_ENABLED
    if (res == kEtcPalErrOk)
    {
      source_state_initted = ((res = sacn_source_state_init()) == kEtcPalErrOk);
      if (!source_state_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE STATE!");
    }

    if (res == kEtcPalErrOk)
    {
      source_initted = ((res = sacn_source_init()) == kEtcPalErrOk);
      if (!source_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE SOURCE!");
    }
#endif  // SACN_SOURCE_ENABLED
  }

#if SACN_DMX_MERGER_ENABLED
  if ((features_to_init & SACN_FEATURE_DMX_MERGER) != 0)
  {
    if (res == kEtcPalErrOk)
    {
      merger_initted = ((res = sacn_dmx_merger_init()) == kEtcPalErrOk);
      if (!merger_initted)
        SACN_LOG_CRIT("FAILED TO INITIALIZE DMX MERGER!");
    }
  }
#endif  // SACN_DMX_MERGER_ENABLED

  if (res == kEtcPalErrOk)
  {
    // Not all of features may be in features_to_init, but we want to increment the counters regardless, so use the
    // former.
    if ((features & SACN_FEATURE_DMX_MERGER) != 0)
      ++sacn_pool_sacn_state.dmx_merger_feature_init_count;

    if ((features & SACN_ALL_NETWORK_FEATURES) == SACN_ALL_NETWORK_FEATURES)
      ++sacn_pool_sacn_state.all_network_features_init_count;
  }
  else
  {
    // Clean up
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
#if SACN_SOURCE_DETECTOR_ENABLED
    if (source_detector_initted)
      sacn_source_detector_deinit();
    if (source_detector_state_initted)
      sacn_source_detector_state_deinit();
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
    if (merge_receiver_initted)
      sacn_merge_receiver_deinit();
#endif  // SACN_MERGE_RECEIVER_ENABLED
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
    if (source_mutex_initted)
      etcpal_mutex_destroy(&sacn_source_mutex);
    if (receiver_state_mutex_initted)
      etcpal_mutex_destroy(&sacn_receiver_state_mutex);
    if (receiver_sockets_mutex_initted)
      etcpal_mutex_destroy(&sacn_receiver_sockets_mutex);

    if (etcpal_netints_initted)
      etcpal_deinit(ETCPAL_FEATURE_NETINTS);
    if (etcpal_timers_initted)
      etcpal_deinit(ETCPAL_FEATURE_TIMERS);
    if (etcpal_sockets_initted)
      etcpal_deinit(ETCPAL_FEATURE_SOCKETS);
    if (etcpal_logging_initted)
      etcpal_deinit(ETCPAL_FEATURE_LOGGING);
    if (log_params_initted)
      sacn_log_params = NULL;
  }

  return res;
}

// NOLINTEND(readability-function-cognitive-complexity)

/**
 * @brief Deinitialize all features of the sACN library.
 *
 * Set all sACN library features back to an uninitialized state if deinit is called as many times as init for
 * a given feature. Calls to deinitialized sACN API functions will fail until sacn_init() is called again for their
 * feature(s).
 *
 * This function is not thread safe with respect to other sACN API functions. Make sure to join your threads that use
 * the APIs before calling this.
 */
void sacn_deinit(void)
{
  sacn_deinit_features(SACN_FEATURES_ALL);
}

/**
 * @brief Deinitialize specific features of the sACN library.
 *
 * Set specific sACN library feature(s) back to an uninitialized state if deinit is called as many times as init for
 * a given feature. Calls to deinitialized sACN API functions will fail until sacn_init() is called again for their
 * feature(s).
 *
 * This function is not thread safe with respect to other sACN API functions. Make sure to join your threads that use
 * the APIs before calling this.
 *
 * @param[in] features Mask of sACN features to deinitialize.
 */
void sacn_deinit_features(sacn_features_t features)
{
  sacn_features_t features_to_deinit = features;

  // Avoid redundant init
  if ((features_to_deinit & SACN_FEATURE_DMX_MERGER) != 0)
  {
    if (sacn_pool_sacn_state.dmx_merger_feature_init_count != 1)
      features_to_deinit = (features_to_deinit & ~SACN_FEATURE_DMX_MERGER);
  }

  if ((features_to_deinit & SACN_ALL_NETWORK_FEATURES) == SACN_ALL_NETWORK_FEATURES)
  {
    if (sacn_pool_sacn_state.all_network_features_init_count != 1)
      features_to_deinit = (features_to_deinit & ~SACN_ALL_NETWORK_FEATURES);
  }

#if SACN_DMX_MERGER_ENABLED
  if ((features_to_deinit & SACN_FEATURE_DMX_MERGER) != 0)
    sacn_dmx_merger_deinit();
#endif  // SACN_DMX_MERGER_ENABLED

  if ((features_to_deinit & SACN_ALL_NETWORK_FEATURES) == SACN_ALL_NETWORK_FEATURES)
  {
#if SACN_SOURCE_ENABLED
    sacn_source_deinit();
    sacn_source_state_deinit();
#endif  // SACN_SOURCE_ENABLED
#if SACN_RECEIVER_ENABLED
    sacn_receiver_deinit();
    sacn_receiver_state_deinit();
    sacn_source_loss_deinit();
#endif  // SACN_RECEIVER_ENABLED
#if SACN_SOURCE_DETECTOR_ENABLED
    sacn_source_detector_deinit();
    sacn_source_detector_state_deinit();
#endif  // SACN_SOURCE_DETECTOR_ENABLED
#if SACN_MERGE_RECEIVER_ENABLED
    sacn_merge_receiver_deinit();
#endif  // SACN_MERGE_RECEIVER_ENABLED
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
    etcpal_mutex_destroy(&sacn_source_mutex);
    etcpal_mutex_destroy(&sacn_receiver_state_mutex);
    etcpal_mutex_destroy(&sacn_receiver_sockets_mutex);

    etcpal_deinit(ETCPAL_FEATURE_NETINTS | ETCPAL_FEATURE_TIMERS | ETCPAL_FEATURE_SOCKETS | ETCPAL_FEATURE_LOGGING);
  }

  sacn_log_params = NULL;

  // Not all of features may be in features_to_deinit, but we want to decrement the counters regardless, so use the
  // former.
  if (((features & SACN_FEATURE_DMX_MERGER) != 0) && (sacn_pool_sacn_state.dmx_merger_feature_init_count > 0))
    --sacn_pool_sacn_state.dmx_merger_feature_init_count;

  if (((features_to_deinit & SACN_ALL_NETWORK_FEATURES) == SACN_ALL_NETWORK_FEATURES) &&
      (sacn_pool_sacn_state.all_network_features_init_count > 0))
  {
    --sacn_pool_sacn_state.all_network_features_init_count;
  }
}

#if SACN_RECEIVER_ENABLED || DOXYGEN

/**
 * @brief Converts a remote source CID to the corresponding handle, or #kSacnRemoteSourceInvalid if not found.
 *
 * This is a simple conversion from a remote source CID to it's corresponding remote source handle. A handle will be
 * returned only if it is a source that has been discovered by a receiver, merge receiver, or source detector.
 *
 * @param[in] source_cid The UUID of the remote source CID.
 * @return The remote source handle, or #kSacnRemoteSourceInvalid if not found.
 */
sacn_remote_source_t sacn_get_remote_source_handle(const EtcPalUuid* source_cid)
{
  sacn_remote_source_t result = kSacnRemoteSourceInvalid;

  if (sacn_receiver_lock())
  {
    result = get_remote_source_handle(source_cid);
    sacn_receiver_unlock();
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

  if ((source_handle == kSacnRemoteSourceInvalid) || !source_cid)
  {
    result = kEtcPalErrInvalid;
  }
  else
  {
    if (sacn_receiver_lock())
    {
      const EtcPalUuid* cid = get_remote_source_cid(source_handle);

      if (cid)
        *source_cid = *cid;
      else
        result = kEtcPalErrNotFound;

      sacn_receiver_unlock();
    }
    else
    {
      result = kEtcPalErrSys;
    }
  }

  return result;
}

#endif  // SACN_RECEIVER_ENABLED

bool sacn_assert_verify_fail(const char* exp, const char* file, const char* func, int line)
{
#if !SACN_LOGGING_ENABLED
  ETCPAL_UNUSED_ARG(exp);
  ETCPAL_UNUSED_ARG(file);
  ETCPAL_UNUSED_ARG(func);
  ETCPAL_UNUSED_ARG(line);
#endif
  SACN_LOG_CRIT("ASSERTION \"%s\" FAILED (FILE: \"%s\" FUNCTION: \"%s\" LINE: %d)", exp ? exp : "", file ? file : "",
                func ? func : "", line);
  SACN_ASSERT(false);
  return false;
}

bool sacn_receiver_lock(void)
{
  return etcpal_mutex_lock(&sacn_receiver_state_mutex);
}

void sacn_receiver_unlock(void)
{
  etcpal_mutex_unlock(&sacn_receiver_state_mutex);
}

bool sacn_receiver_sockets_lock(void)
{
  return etcpal_mutex_lock(&sacn_receiver_sockets_mutex);
}

void sacn_receiver_sockets_unlock(void)
{
  etcpal_mutex_unlock(&sacn_receiver_sockets_mutex);
}

bool sacn_source_lock(void)
{
  return etcpal_mutex_lock(&sacn_source_mutex);
}

void sacn_source_unlock(void)
{
  etcpal_mutex_unlock(&sacn_source_mutex);
}

bool sacn_sem_post(etcpal_sem_t* sem)
{
  return etcpal_sem_post(sem);
}

bool sacn_sem_timed_wait(etcpal_sem_t* sem, int timeout_ms)
{
  return etcpal_sem_timed_wait(sem, timeout_ms);
}

bool sacn_initialized(sacn_features_t features)
{
  if (((features & SACN_FEATURE_DMX_MERGER) != 0) && (sacn_pool_sacn_state.dmx_merger_feature_init_count == 0))
    return false;

  if (((features & SACN_ALL_NETWORK_FEATURES) != 0) && (sacn_pool_sacn_state.all_network_features_init_count == 0))
    return false;

  return true;
}
