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
 * SourceDetector my_source_detector;
 * MyNotifyHandler my_notify;  // MyNotifyHandler is an application-defined class for implementing notifications.
 *
 * // If you want to specify specific network interfaces to use:
 * etcpal::Error startup_result = my_source_detector.Startup(my_notify, my_netints);
 * // Or, if you just want to use all network interfaces:
 * etcpal::Error startup_result = my_source_detector.Startup(my_notify, std::vector<SacnMcastInterface>());
 * // Check startup_result here...
 * 
 * // Now the thread is running and your callbacks will handle application-side processing.
 * 
 * // What if your network interfaces change? Update my_netints and call this:
 * etcpal::Error reset_result = my_source_detector.ResetNetworking(my_netints);
 * // Check reset_result here...
 * 
 * // To destroy a source detector, call this:
 * my_source_detector.Shutdown();
 * 
 * // During application shutdown, everything can be cleaned up by calling sacn::Deinit.
 * sacn::Deinit();
 * @endcode
 *
 * Callback demonstrations:
 * @code
 * void MyNotifyHandler::HandleSourceUpdated(const etcpal::Uuid& cid, const std::string& name,
                                             const std::vector<uint16_t>& sourced_universes)
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
 * void MyNotifyHandler::HandleSourceExpired(const etcpal::Uuid& cid, const std::string& name)
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
  /** A handle type used by the sACN library to identify source detector instances. */
  using Handle = sacn_source_detector_t;
  /** An invalid Handle value. */
  static constexpr Handle kInvalidHandle = SACN_SOURCE_DETECTOR_INVALID;

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
     * The list of sourced universes is guaranteed by the protocol to be numerically sorted.
     *
     * @param[in] cid The CID of the source.
     * @param[in] name The UTF-8 name string.
     * @param[in] sourced_universes Numerically sorted array of the currently sourced universes.  Will be empty if the
     * source is not currently transmitting any universes.
     */
    virtual void HandleSourceUpdated(const etcpal::Uuid& cid, const std::string& name,
                                     const std::vector<uint16_t>& sourced_universes) = 0;

    /**
     * @brief Notify that a source is no longer transmitting Universe Discovery messages.
     *
     * @param[in] cid The CID of the source.
     * @param[in] name The UTF-8 name string.
     */
    virtual void HandleSourceExpired(const etcpal::Uuid& cid, const std::string& name) = 0;

    /**
     * @brief Notify that the module has run out of memory to track universes or sources
     *
     * If #SACN_DYNAMIC_MEM was defined to 1 when sACN was compiled (the default on non-embedded
     * platforms), and the configuration you pass to sacn_source_detector_create() has source_count_max and
     * universes_per_source_max set to #SACN_SOURCE_DETECTOR_INFINITE, this callback will never be called.
     *
     * If #SACN_DYNAMIC_MEM was defined to 0 when sACN was compiled, source_count_max and universes_per_source_max are
     * ignored and #SACN_SOURCE_DETECTOR_MAX_SOURCES and #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE are used
     * instead.
     *
     * This callback is rate-limited: it will only be called when the first universe discovery packet is received that
     * takes the module beyond a memory limit.  After that, it will not be called until the number of sources or
     * universes has dropped below the limit and hits it again.
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

    /** The maximum number of sources this detector will record.  It is recommended that applications using dynamic
       memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when configured to use
       static memory -- #SACN_SOURCE_DETECTOR_MAX_SOURCES is used instead.*/
    int source_count_max{SACN_SOURCE_DETECTOR_INFINITE};

    /** The maximum number of universes this detector will record for a source.  It is recommended that applications
       using dynamic memory use #SACN_SOURCE_DETECTOR_INFINITE for this value. This parameter is ignored when
       configured to use static memory -- #SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE is used instead.*/
    int universes_per_source_max{SACN_SOURCE_DETECTOR_INFINITE};

    /** Create default data structure. */
    Settings() = default;
  };

  SourceDetector() = default;
  SourceDetector(const SourceDetector& other) = delete;
  SourceDetector& operator=(const SourceDetector& other) = delete;
  SourceDetector(SourceDetector&& other) = default;            /**< Move a detector instance. */
  SourceDetector& operator=(SourceDetector&& other) = default; /**< Move a detector instance. */

  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler,
                        std::vector<SacnMcastInterface>& netints = kAllInterfaces);

  etcpal::Error Startup(NotifyHandler& notify_handler, std::vector<SacnMcastInterface>& netints = kAllInterfaces);
  void Shutdown();
  etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints = kAllInterfaces);

  constexpr Handle handle() const;

private:
  SacnSourceDetectorConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  /** An internal shortcut for selecting all network interfaces. **/
  static std::vector<SacnMcastInterface> kAllInterfaces;

  Handle handle_{kInvalidHandle};
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void SourceDetectorCbSourceUpdated(sacn_source_detector_t handle, const EtcPalUuid* cid,
                                                     const char* name, const uint16_t* sourced_universes,
                                                     size_t num_sourced_universes, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context && cid && name)
  {
    std::vector<uint16_t> sourced_vec;
    if (sourced_universes && (num_sourced_universes > 0))
      sourced_vec.assign(sourced_universes, sourced_universes + num_sourced_universes);
    static_cast<SourceDetector::NotifyHandler*>(context)->HandleSourceUpdated(*cid, name, sourced_vec);
  }
}

extern "C" inline void SourceDetectorCbSourceExpired(sacn_source_detector_t handle, const EtcPalUuid* cid,
                                                        const char* name, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context && cid && name)
  {
    static_cast<SourceDetector::NotifyHandler*>(context)->HandleSourceExpired(*cid, name);
  }
}

extern "C" inline void SourceDetectorCbMemoryLimitExceeded(sacn_source_detector_t handle, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

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
 * @brief Start a new sACN Source Detector.
 *
 * Note that a detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN Source Detector to be created.
 * @param[in] notify_handler The callback handler for the sACN Source Detector to be created.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't
 * modified.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this detector.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                                std::vector<SacnMcastInterface>& netints)
{
  SacnSourceDetectorConfig config = TranslateConfig(settings, notify_handler);

  if (netints.empty())
    return sacn_source_detector_create(&config, &handle_, NULL, 0);

  return sacn_source_detector_create(&config, &handle_, netints.data(), netints.size());
}

/**
 * @brief Start a new sACN Source Detector with default settings.
 *
 * This is an override of Startup that doesn't require a Settings parameter, since the fields in that
 * structure are completely optional.
 *
 * Note that a detector is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] notify_handler The callback handler for the sACN Source Detector to be created.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't
 * modified.
 * @return #kEtcPalErrOk: Detector created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this detector.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::Startup(NotifyHandler& notify_handler, std::vector<SacnMcastInterface>& netints)
{
  return Startup(Settings(), notify_handler, netints);
}

/**
 * @brief Destroy a sACN Source Detector instance.
 *
 * @return #kEtcPalErrOk: Detector destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Internal handle does not correspond to a valid detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline void SourceDetector::Shutdown()
{
  sacn_source_detector_destroy(handle_);
  handle_ = kInvalidHandle;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN Source Detector.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the detector will continue as if nothing had changed. New sources could be
 * discovered, or old sources could expire.
 * If this call fails, the caller must call Shutdown() for this class instance, because it may
 * be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't
 * modified.
 * @return #kEtcPalErrOk: Network changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Internal handle does not correspond to a valid detector.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error SourceDetector::ResetNetworking(std::vector<SacnMcastInterface>& netints)
{
  if (netints.empty())
    return sacn_source_detector_reset_networking(handle_, nullptr, 0);
  else
    return sacn_source_detector_reset_networking(handle_, netints.data(), netints.size());
}

/**
 * @brief Get the current handle to the underlying C source detector.
 *
 * @return The handle or SourceDetector::kInvalidHandle.
 */
inline constexpr SourceDetector::Handle SourceDetector::handle() const
{
  return handle_;
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
    settings.universes_per_source_max
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_SOURCE_DETECTOR_H_
