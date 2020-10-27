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

#ifndef SACN_CPP_UNIVERSE_DISCOVERY_H_
#define SACN_CPP_UNIVERSE_DISCOVERY_H_

/**
 * @file sacn/cpp/universe_discovery.h
 * @brief C++ wrapper for the sACN Universe Discovery API
 */

#include "sacn/universe_discovery.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"

/**
* @defgroup sacn_universe_discovery_cpp sACN Universe Discovery API
* @ingroup sacn_cpp_api
* @brief A C++ wrapper for the sACN Universe Discovery API
*/

namespace sacn
{
/**
 *@ingroup sacn_universe_discovery_cpp
 *@brief An instance of sACN Universe Discovery functionality.
 */
// CHRISTIAN TODO: FILL OUT THIS COMMENT MORE WITH SAMPLE CODE.

class UniverseDiscovery
{
public:
  /** A handle type used by the sACN library to identify Universe Discovery listener instances. */
  using Handle = sacn_universe_discovery_t;
  /** An invalid Handle value. */
  static constexpr Handle kInvalidHandle = SACN_UNIVERSE_DISCOVERY_INVALID;

  /**
   * @ingroup sacn_universe_discovery_cpp
   * @brief A base class for a class that receives notification callbacks from a sACN Universe Discovery listener.
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
     * @param[in] sourced_universes Numerically sorted array of the currently sourced universes.  Will be NULL if the
     * source is not currently transmitting any universes.
     * @param[in] num_sourced_universe Size of the sourced_universes array.  Will be 0 if the source is not currently
     * transmitting any universes.
     */
    virtual void HandleUpdateSource(const etcpal::Uuid& cid, const std::string& name, const uint16_t* sourced_universes,
                                    size_t num_sourced_sources) = 0;

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
     * platforms), and the configuration you pass to sacn_universe_discovery_create() has source_count_max and
     * universes_per_source_max set to #SACN_UNIVERSE_DISCOVERY_INFINITE, this callback will never be called and may be
     * set to NULL.
     *
     * if #SACN_DYNAMIC_MEM was defined to 0 when sACN was compiled, source_count_max is ignored and
     * #SACN_UNIVERSE_DISCOVERY_MAX_SOURCES and #SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_SOURCE are used instead.
     *
     * This callback is rate-limited: it will only be called when the first universe discovery packet is received that
     * takes the module beyond a memory limit.  After that, it will not be called until the number of sources or
     * universes has dropped below the limit and hits it again.
     */
    virtual void HandleMemoryLimitExceeded() = 0;
  };

  /**
   * @ingroup sacn_universe_discovery_cpp
   * @brief A set of configuration settings that a universe discovery listener needs to initialize.
   */
  struct Settings
  {
    /********* Required values **********/

    /********* Optional values **********/

    /** The maximum number of sources this listener will record.  It is recommended that applications using dynamic
       memory use #SACN_UNIVERSE_DISCOVERY_INFINITE for this value. This parameter is ignored when configured to use
       static memory -- #SACN_UNIVERSE_DISCOVERY_MAX_SOURCES is used instead.*/
    size_t source_count_max{SACN_UNIVERSE_DISCOVERY_INFINITE};

    /** The maximum number of universes this listener will record for a source.  It is recommended that applications
       using dynamic memory use #SACN_UNIVERSE_DISCOVERY_INFINITE for this value. This parameter is ignored when
       configured to use static memory -- #SACN_UNIVERSE_DISCOVERY_MAX_SOURCES is used instead.*/
    size_t universes_per_source_max{SACN_UNIVERSE_DISCOVERY_INFINITE};

    /** Create default data structure. */
    Settings() = default;
  };

  UniverseDiscovery() = default;
  UniverseDiscovery(const UniverseDiscovery& other) = delete;
  UniverseDiscovery& operator=(const UniverseDiscovery& other) = delete;
  UniverseDiscovery(UniverseDiscovery&& other) = default;            /**< Move a listener instance. */
  UniverseDiscovery& operator=(UniverseDiscovery&& other) = default; /**< Move a listener instance. */

  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler,
                        std::vector<SacnMcastInterface>& netints);

  etcpal::Error Startup(NotifyHandler& notify_handler, std::vector<SacnMcastInterface>& netints);
  void Shutdown();
  etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints);

  constexpr Handle handle() const;

private:
  SacnUniverseDiscoveryConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_{kInvalidHandle};
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void UniverseDiscoveryCbUpdateSource(sacn_universe_discovery_t handle, const EtcPalUuid* cid,
                                                       const char* name, const uint16_t* sourced_universes,
                                                       size_t num_sourced_universes, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context && cid && name)
  {
    static_cast<UniverseDiscovery::NotifyHandler*>(context)->HandleUpdateSource(*cid, name, sourced_universes,
                                                                                num_sourced_universes);
  }
}

extern "C" inline void UniverseDiscoveryCbSourceExpired(sacn_universe_discovery_t handle, const EtcPalUuid* cid,
                                                        const char* name, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context && cid && name)
  {
    static_cast<UniverseDiscovery::NotifyHandler*>(context)->HandleSourceExpired(*cid, name);
  }
}

extern "C" inline void UniverseDiscoveryCbMemoryLimitExceeded(sacn_universe_discovery_t handle, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context)
  {
    static_cast<UniverseDiscovery::NotifyHandler*>(context)->HandleMemoryLimitExceeded();
  }
}

};  // namespace internal

/**
 * @endcond
 */

/**
 * @brief Start a new sACN Universe Discovery listener.
 *
 * Note that a listener is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] Settings Configuration parameters for the sACN Universe Discovery listener to be created.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If NULL, all available interfaces are tried.
 * @return #kEtcPalErrOk: Listener created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this listener.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error UniverseDiscovery::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                                std::vector<SacnMcastInterface>& netints)
{
  SacnUniverseDiscoveryConfig config = TranslateConfig(settings, notify_handler);

  if (netints.empty())
    return sacn_universe_discovery_create(&config, &handle_, NULL, 0);

  return sacn_universe_discovery_create(&config, &handle_, netints.data(), netints.size());
}

/**
 * @brief Start a new sACN Universe Discovery listener with default settings.
 *
 * This is an override of Startup that doesn't require a Settings parameter, since the fields in that
 * structure are completely optional.
 *
 * Note that a listener is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't
 * modified.
 * @return #kEtcPalErrOk: Listener created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this listener.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error UniverseDiscovery::Startup(NotifyHandler& notify_handler, std::vector<SacnMcastInterface>& netints)
{
  return Startup(Settings(), notify_handler, netints);
}

/**
 * @brief Destroy a sACN Universe Discovery listener instance.
 *
 * @return #kEtcPalErrOk: Listener destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Internal handle does not correspond to a valid listener.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline void UniverseDiscovery::Shutdown()
{
  sacn_universe_discovery_destroy(handle_);
  handle_ = kInvalidHandle;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN Universe Discovery listener.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the listener will continue as if nothing had changed. New sources could be
 * discovered, or old sources could expire.
 * If this call fails, the caller must call Shutdown() for this class instance, because it may
 * be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Network changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Internal handle does not correspond to a valid listener.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error UniverseDiscovery::ResetNetworking(std::vector<SacnMcastInterface>& netints)
{
  if (netints.empty())
    return sacn_universe_discovery_reset_networking(handle_, nullptr, 0);
  else
    return sacn_universe_discovery_reset_networking(handle_, netints.data(), netints.size());
}

/**
 * @brief Get the current handle to the underlying C universe discovery listener.
 *
 * @return The handle or UniverseDiscovery::kInvalidHandle.
 */
inline constexpr UniverseDiscovery::Handle UniverseDiscovery::handle() const
{
  return handle_;
}

inline SacnUniverseDiscoveryConfig UniverseDiscovery::TranslateConfig(const Settings& settings,
                                                                      NotifyHandler& notify_handler)
{
  // clang-format off
  SacnUniverseDiscoveryConfig config = {
    {
      internal::UniverseDiscoveryCbUpdateSource,
      internal::UniverseDiscoveryCbSourceExpired,
      internal::UniverseDiscoveryCbMemoryLimitExceeded,
      &notify_handler
    },
    settings.source_count_max,
    settings.universes_per_source_max
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_UNIVERSE_DISCOVERY_H_
