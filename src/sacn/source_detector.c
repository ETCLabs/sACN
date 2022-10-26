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

#include "sacn/source_detector.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "sacn/private/mem.h"
#include "sacn/private/receiver_state.h"
#include "sacn/private/source_detector.h"
#include "sacn/private/source_detector_state.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

#if SACN_SOURCE_DETECTOR_ENABLED || DOXYGEN

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Source Detector module. Internal function called from sacn_init(). */
etcpal_error_t sacn_source_detector_init(void)
{
  return kEtcPalErrOk;  // Nothing to do here.
}

/* Deinitialize the sACN Source Detector module. Internal function called from sacn_deinit(). */
void sacn_source_detector_deinit(void)
{
  // Nothing to do here.
}

/**
 * @brief Initialize an sACN Source Detector Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_detector_config_init(SacnSourceDetectorConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(SacnSourceDetectorConfig));
  }
}

/**
 * @brief Create the sACN Source Detector.
 *
 * Note that the detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] config Configuration parameters for the sACN source detector.
 * @param[in, out] netint_config Optional. If non-NULL, this is the list of interfaces the application wants to use, and
 * the status codes are filled in.  If NULL, all available interfaces are tried.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for the detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_detector_create(const SacnSourceDetectorConfig* config,
                                           const SacnNetintConfig* netint_config)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;
  else if (!config || !config->callbacks.source_updated || !config->callbacks.source_expired)
    res = kEtcPalErrInvalid;

  if (res == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      SacnSourceDetector* source_detector = NULL;
      res = add_sacn_source_detector(config, netint_config, &source_detector);

      if (res == kEtcPalErrOk)
        res = assign_source_detector_to_thread(source_detector);

      if ((res != kEtcPalErrOk) && source_detector)
      {
        remove_source_detector_from_thread(source_detector);
        remove_sacn_source_detector();
      }

      sacn_unlock();
    }
    else
    {
      res = kEtcPalErrSys;
    }
  }

  return res;
}

/**
 * @brief Destroy the sACN Source Detector.
 *
 */
void sacn_source_detector_destroy()
{
  if (sacn_initialized() && sacn_lock())
  {
    SacnSourceDetector* detector = get_sacn_source_detector();
    if (detector)
    {
      remove_source_detector_from_thread(detector);
      remove_sacn_source_detector();
    }

    sacn_unlock();
  }
}

/**
 * @brief Updates the source detector system network interfaces. Also resets the underlying network sockets for the sACN
 * Source Detector if it was created.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the source detector API will be limited to (the list passed into sacn_init(), if any,
 * is overridden for the source detector API, but not the other APIs). The source detector is then set to those
 * interfaces.
 *
 * After this call completes successfully, the detector will continue as if nothing had changed. New sources could be
 * discovered, or old sources could expire.
 * If this call fails, the caller must call sacn_source_detector_destroy, because the detector may be in an invalid
 * state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] sys_netint_config Optional. If non-NULL, this is the list of system interfaces the source detector
 * API will be limited to, and the status codes are filled in.  If NULL, the source detector API is allowed to use all
 * available system interfaces.
 * @return #kEtcPalErrOk: Network changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_detector_reset_networking(const SacnNetintConfig* sys_netint_config)
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!sacn_initialized())
    res = kEtcPalErrNotInit;

  if (res == kEtcPalErrOk)
  {
    if (sacn_lock())
    {
      res = sacn_sockets_reset_source_detector(sys_netint_config);

      if (res == kEtcPalErrOk)
      {
        SacnSourceDetector* detector = get_sacn_source_detector();
        if (detector)
        {
          // All current sockets need to be removed before adding new ones.
          remove_source_detector_sockets(detector, kQueueSocketCleanup);

          res = sacn_initialize_source_detector_netints(&detector->netints, NULL);
          if (res == kEtcPalErrOk)
            res = add_source_detector_sockets(detector);
        }
      }

      sacn_unlock();
    }
    else
    {
      res = kEtcPalErrSys;
    }
  }

  return res;
}

/**
 * @brief Obtain the source detector's network interfaces.
 *
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the source detector. If this is greater than netints_size, then
 * only netints_size addresses were written to the netints array. If the source detector has not been created yet, 0 is
 * returned.
 */
size_t sacn_source_detector_get_network_interfaces(EtcPalMcastNetintId* netints, size_t netints_size)
{
  size_t total_num_network_interfaces = 0;

  if (sacn_lock())
  {
    SacnSourceDetector* detector = get_sacn_source_detector();
    if (detector)
      total_num_network_interfaces = get_source_detector_netints(detector, netints, netints_size);

    sacn_unlock();
  }

  return total_num_network_interfaces;
}

#endif  // SACN_SOURCE_DETECTOR_ENABLED || DOXYGEN
