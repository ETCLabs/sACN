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

/*********** CHRISTIAN's BIG OL' TODO LIST: *************************************
 - Implement this & get documentation in place, including the overal C & C++ "how to use" comments.
 - Clean up any constants for this API in opts.h.  For example, any of the universe discovery thread priorities or
 receive timeouts.
 - Test c & c++
 - Make sure everything works with static & dynamic memory.
 - Example app for both C & C++.
 - Make the example listener use the new api.
 - This entire project should build without warnings!!
 - Make sure the functionality for create & reset_networking work with and without good_interfaces, in all combinations
 (nill, small array, large array, etc).
*/
////////////////////////////////////////////

#include "sacn/universe_discovery.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "sacn/private/universe_discovery.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private constants *****************************/

static const EtcPalThreadParams kUniverseDiscoveryThreadParams = {SACN_UNIVERSE_DISCOVERY_THREAD_PRIORITY,
                                                                  SACN_UNIVERSE_DISCOVERY_THREAD_STACK,
                                                                  "sACN Universe Discovery Thread", NULL};

/****************************** Private macros *******************************/

/**************************** Private variables ******************************/

/*********************** Private function prototypes *************************/

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Universe Discovery module. Internal function called from sacn_init(). */
etcpal_error_t sacn_universe_discovery_init(void)
{
  // TODO CHRISTIAN
  etcpal_error_t res = kEtcPalErrNotImpl;

  return res;
}

/* Deinitialize the sACN Universe Discovery module. Internal function called from sacn_deinit(). */
void sacn_universe_discovery_deinit(void)
{
  // TODO CHRISTIAN
}

/**
 * @brief Initialize an sACN Universe Discovery Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_universe_discovery_config_init(SacnUniverseDiscoveryConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(SacnUniverseDiscoveryConfig));
  }
}

/**
 * @brief Create a new sACN Universe Discovery listener.
 *
 * Note that a listener is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] config Configuration parameters for the sACN Universe Discovery listener to be created.
 * @param[out] handle Filled in on success with a handle to the listener.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Listener created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this listener.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_universe_discovery_create(const SacnUniverseDiscoveryConfig* config,
                                              sacn_universe_discovery_t* handle, SacnMcastInterface* netints,
                                              size_t num_netints)
{
  // TODO CHRISTIAN
  // create starts a thread that does all the receive & processing work for packets.
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Destroy a sACN Universe Discovery instance.
 *
 *
 * @param[in] handle Handle to the listener to destroy.
 */
void sacn_universe_discovery_destroy(sacn_universe_discovery_t handle)
{
  // TODO CHRISTIAN
  // Shutdown the thread if it is for the last listener.
  ETCPAL_UNUSED_ARG(handle);
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN Universe Discovery listener.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the listener will continue as if nothing had changed. New sources could be
 * discovered, or old sources could expire.
 * If this call fails, the caller must call sacn_universe_discovery_destroy for the listener, because the listener may
 * be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the listener for which to reset the networking.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Network changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid listener.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_universe_discovery_reset_networking(sacn_universe_discovery_t handle, SacnMcastInterface* netints,
                                                        size_t num_netints)
{
  // TODO CHRISTIAN
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);

  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  return kEtcPalErrNotImpl;
}
