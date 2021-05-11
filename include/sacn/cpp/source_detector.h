/******************************************************************************
 * Copyright 2021 ETC Inc.
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

#ifndef SACN_CPP_SOURCE_DETECTOR_H_
#define SACN_CPP_SOURCE_DETECTOR_H_

/**
 * @file sacn/cpp/source_detector.h
 * @brief C++ wrapper for the sACN Source Detector API
 */

#include <vector>

#include "sacn/source_detector.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"

/**
* @defgroup sacn_source_detector_cpp sACN Source Detector API
* @ingroup sacn_cpp_api
* @brief A C++ wrapper for the sACN Source Detector API
*/

namespace sacn
{
/**
 * @ingroup sacn_source_detector_cpp
 * @brief An instance of sACN Source Detector functionality.
 *
 * sACN sources often periodically send Universe Discovery packets to announce what universes they are sourcing.
 * Use this API to monitor such traffic for your own needs.
 *
 * There can only ever be one instance of the source detector (thus the static interface), but that instance still needs
 * to be created and can be destroyed.
 * 
 * Usage:
 * @code
 * #include "sacn/cpp/source_detector.h"
 *
 * EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
 * // Initialize log_params...
 *
 * etcpal::Error init_result = sacn::Init(&log_params);
 * // Or, to init without worrying about logs from the sACN library...
 * etcpal::Error init_result = sacn::Init();
 * 
 * std::vector<SacnMcastInterface> my_netints;
 * // Assuming my_netints is initialized by the application...
 *
 * MyNotifyHandler my_notify;  // MyNotifyHandler is an application-defined class for implementing notifications.
 *
 * // If you want to specify specific network interfaces to use:
 * etcpal::Error startup_result = sacn::SourceDetector::Startup(my_notify, my_netints);
 * // Or, if you just want to use all network interfaces:
 * etcpal::Error startup_result = sacn::SourceDetector::Startup(my_notify);
 * // Check startup_result here...
 * 
 * // Now the thread is running and your callbacks will handle application-side processing.
 * 
 * // What if your network interfaces change? Update my_netints and call this:
 * etcpal::Error reset_result = sacn::SourceDetector::ResetNetworking(my_netints);
 * // Check reset_result here...
 * 
 * // To destroy the source detector, call this:
 * sacn::SourceDetector::Shutdown();
 * 
 * // During application shutdown, everything can be cleaned up by calling sacn::Deinit.
 * sacn::Deinit();
 * @endcode
 *
 * Callback demonstrations:
 * @code
 * void MyNotifyHandler::HandleSourceUpdated(sacn::RemoteSourceHandle handle, const etcpal::Uuid& cid,
                                             const std::string& name, const std::vector<uint16_t>& sourced_universes)
 * {
 *   std::cout << "Source Detector: Source " << cid.ToString() << " (name " << name << ") ";
 *   if(!sourced_universes.empty())
 *   {
 *     std::cout << "is active on these universes: ";
 *     for(uint16_t univ : sourced_universes)
 *       std::cout << univ << " ";
 *     std::cout << "\n";
 *   }
 *   else
 *   {
 *     std::cout << "is not active on any universes.\n";
 *   }
 * }
 *
 * void MyNotifyHandler::HandleSourceExpired(sacn::RemoteSourceHandle handle, const etcpal::Uuid& cid,
 *                                           const std::string& name)
 * {
 *   std::cout << "Source Detector: Source " << cid.ToString() << " (name " << name << ") has expired.\n";
 * }
 *
 * void MyNotifyHandler::HandleMemoryLimitExceeded()
 * {
 *   std::cout << "Source Detector: Source/universe limit exceeded!\n";
 * }
 * @endcode
 */

class SourceDetector
{
public:

  /**
   * @ingroup sacn_source_detector_cpp
   * @brief A base class for a class that receives notification callbacks from a sACN Source Detector.
   */
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

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
     * @param[in] name The UTF-8 name string.
     * @param[in] sourced_universes Numerically sorted array of the currently sourced universes.  Will be empty if the
     * source is not currently transmitting any universes.
     */
    virtual void HandleSourceUpdated(RemoteSourceHandle handle, const etcpal::Uuid& cid, const std::string& name,
                                     const std::vector<uint16_t>& sourced_universes) = 0;

    /**
     * @brief Notify that a source is no longer transmitting Universe Discovery messages.
     *
     * @param[in] handle The handle uniquely identifying the source.
     * @param[in] cid The CID of the source.
     * @param[in] name The UTF-8 name string.
     */
    virtual void HandleSourceExpired(RemoteSourceHandle handle, const etcpal::Uuid& cid, const std::string& name) = 0;

    /**
     * @brief Notify that the module has run out of memory to track universes or sources.
     *
     * If #SACN_DYNAMIC_MEM was defined to 1 when sACN was compiled (the default on non-embedded platforms), and the
     * configuration you pass to Startup() has source_count_max and universes_per_source_max set to
     * #SACN_SOURCE_DETECTOR_INFINITE, this callback will never be called (except for the rare case where a heap
     * allocation function fails).
     *
     * If #SACN_DYNAMIC_MEM was defined to 0 when sACN was compiled, source_count_max and universes_per_source_max are
     * ignored and #SACN_SOURCE_DETECTOR_MAX_SOURCES and #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE are used
     * instead.
     *
     * This callback is rate-limited: it will only be called the first time a source or universe limit is exceeded.
     * After that, it will not be called until the number of sources or universes has dropped below their limit and hits
     * it again.
     * 
     */
    virtual void HandleMemoryLimitExceeded() {}
  };

  /**
   * @ingroup sacn_source_detector_cpp
   * @brief A set of configuration settings that a source detector needs to initialize.
   */
  struct Settings
  {
    /********* Required values **********/

    /********* Optional values **********/

    /** The maximum number of sources the detector will record.  It is recommended that applications using dynamic
       memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when configured to use
       static memory -- #SACN_SOURCE_DETECTOR_MAX_SOURCES is used instead.*/
    int source_count_max{SACN_SOURCE_DETECTOR_INFINITE};

    /** The maximum number of universes the detector will record for a source.  It is recommended that applications
       using dynamic memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when
       configured to use static memory -- #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE is used instead.*/
    int universes_per_source_max{SACN_SOURCE_DETECTOR_INFINITE};

    /** What IP networking the source detector will support. */
    sacn_ip_support_t ip_supported{kSacnIpV4AndIpV6};

    /** Create default data structure. */
    Settings() = default;
  };

  SourceDetector() = delete;

  static etcpal::Error Startup(NotifyHandler& notify_handler);
  static etcpal::Error Startup(NotifyHandler& notify_handler, std::vector<SacnMcastInterface>& netints);
  static etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler);
  static etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler,
                               std::vector<SacnMcastInterface>& netints);
  static void Shutdown();
  static etcpal::Error ResetNetworking();
  static etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints);
  static std::vector<EtcPalMcastNetintId> GetNetworkInterfaces();

private:
  static SacnSourceDetectorConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void SourceDetectorCbSourceUpdated(sacn_remote_source_t handle, const EtcPalUuid* cid,
                                                     const char* name, const uint16_t* sourced_universes,
                                                     size_t num_sourced_universes, void* context)
{
  if (context && cid && name)
  {
    std::vector<uint16_t> sourced_vec;
    if (sourced_universes && (num_sourced_universes > 0))
      sourced_vec.assign(sourced_universes, sourced_universes + num_sourced_universes);
    static_cast<SourceDetector::NotifyHandler*>(context)->HandleSourceUpdated(handle, *cid, name, sourced_vec);
  }
}

extern "C" inline void SourceDetectorCbSourceExpired(sacn_remote_source_t handle, const EtcPalUuid* cid,
                                                     const char* name, void* context)
{
  if (context && cid && name)
  {
    static_cast<SourceDetector::NotifyHandler*>(context)->HandleSourceExpired(handle, *cid, name);
  }
}

extern "C" inline void SourceDetectorCbMemoryLimitExceeded(void* context)
{
  if (context)
  {
    static_cast<SourceDetector::NotifyHandler*>(context)->HandleMemoryLimitExceeded();
  }
}

};  // namespace internal

/**
 * @endcond
 */

/**
 * @brief Start the sACN Source Detector with default settings.
 *
 * This is an override of Startup that has default settings for the configuration and will use all network interfaces.
 *
 * Note that the detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] notify_handler The callback handler for the sACN Source Detector to be created.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for the detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::Startup(NotifyHandler& notify_handler)
{
  std::vector<SacnMcastInterface> netints;
  return Startup(Settings(), notify_handler, netints);
}

/**
 * @brief Start the sACN Source Detector with default settings.
 *
 * This is an override of Startup that doesn't require a Settings parameter, since the fields in that
 * structure are completely optional.
 *
 * Note that the detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] notify_handler The callback handler for the sACN Source Detector to be created.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for the detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::Startup(NotifyHandler& notify_handler, std::vector<SacnMcastInterface>& netints)
{
  return Startup(Settings(), notify_handler, netints);
}

/**
 * @brief Start the sACN Source Detector.
 *
 * This is an override of Startup that uses all network interfaces.
 *
 * Note that the detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN Source Detector to be created.
 * @param[in] notify_handler The callback handler for the sACN Source Detector to be created.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for the detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::Startup(const Settings& settings, NotifyHandler& notify_handler)
{
  std::vector<SacnMcastInterface> netints;
  return Startup(settings, notify_handler, netints);
}

/**
 * @brief Start the sACN Source Detector.
 *
 * Note that the detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN Source Detector to be created.
 * @param[in] notify_handler The callback handler for the sACN Source Detector to be created.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for the detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                                std::vector<SacnMcastInterface>& netints)
{
  SacnSourceDetectorConfig config = TranslateConfig(settings, notify_handler);

  if (netints.empty())
    return sacn_source_detector_create(&config, NULL, 0);

  return sacn_source_detector_create(&config, netints.data(), netints.size());
}

/**
 * @brief Destroy the sACN Source Detector.
 *
 * @return #kEtcPalErrOk: Detector destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: The detector has not yet been created.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline void SourceDetector::Shutdown()
{
  sacn_source_detector_destroy();
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN Source Detector.
 *
 * This is an override of ResetNetworking that uses all network interfaces.
 * 
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the detector will continue as if nothing had changed. New sources could be
 * discovered, or old sources could expire.
 * If this call fails, the caller must call Shutdown(), because the source detector may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @return #kEtcPalErrOk: Network changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: The detector has not yet been created.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::ResetNetworking()
{
  std::vector<SacnMcastInterface> netints;
  return ResetNetworking(netints);
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN Source Detector.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the detector will continue as if nothing had changed. New sources could be
 * discovered, or old sources could expire.
 * If this call fails, the caller must call Shutdown(), because the source detector may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Network changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: The detector has not yet been created.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::ResetNetworking(std::vector<SacnMcastInterface>& netints)
{
  if (netints.empty())
    return sacn_source_detector_reset_networking(nullptr, 0);
  else
    return sacn_source_detector_reset_networking(netints.data(), netints.size());
}

/**
 * @brief Obtain the source detector's network interfaces.
 *
 * @return A vector of the source detector's network interfaces.
 */
inline std::vector<EtcPalMcastNetintId> SourceDetector::GetNetworkInterfaces()
{
  // This uses a guessing algorithm with a while loop to avoid race conditions.
  std::vector<EtcPalMcastNetintId> netints;
  size_t size_guess = 4u;
  size_t num_netints = 0u;

  do
  {
    netints.resize(size_guess);
    num_netints = sacn_source_detector_get_network_interfaces(netints.data(), netints.size());
    size_guess = num_netints + 4u;
  } while (num_netints > netints.size());

  netints.resize(num_netints);
  return netints;
}

inline SacnSourceDetectorConfig SourceDetector::TranslateConfig(const Settings& settings, NotifyHandler& notify_handler)
{
  // clang-format off
  SacnSourceDetectorConfig config = {
    {
      internal::SourceDetectorCbSourceUpdated,
      internal::SourceDetectorCbSourceExpired,
      internal::SourceDetectorCbMemoryLimitExceeded,
      &notify_handler
    },
    settings.source_count_max,
    settings.universes_per_source_max,
    settings.ip_supported
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_SOURCE_DETECTOR_H_
