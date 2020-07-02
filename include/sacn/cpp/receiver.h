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

#ifndef SACN_CPP_RECEIVER_H_
#define SACN_CPP_RECEIVER_H_

/// @file sacn/cpp/receiver.h
/// @brief C++ wrapper for the sACN Receiver API

#include "sacn/receiver.h"
#include "etcpal/cpp/inet.h"

/// @defgroup sacn_receiver_cpp sACN Receiver API
/// @ingroup sacn_cpp_api
/// @brief A C++ wrapper for the sACN Receiver API

namespace sacn
{
/// @ingroup sacn_receiver_cpp
/// @brief An instance of sACN Receiver functionality.
///
/// TODO: FILL OUT THIS COMMENT MORE -- DO WE NEED A using_receiver.md???
class Receiver
{
public:
  /// A handle type used by the sACN library to identify receiver instances.
  using Handle = sacn_receiver_t;
  /// An invalid Handle value.
  static constexpr Handle kInvalidHandle = SACN_RECEIVER_INVALID;

  /// @ingroup sacn_receiver_cpp
  /// @brief A base class for a class that receives notification callbacks from a sACN receiver.
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /// @brief Notify that one or more sources have been found.
    /// @param handle Handle to the receiver instance for which sources were found.
    /// @param found_sources Array of structs describing the source or sources that have been found with their current
    /// values.
    /// @param] num_sources_found Size of the found_sources array.
    virtual void HandleSourcesFound(Handle handle, const SacnFoundSource* found_sources, size_t num_found_sources) = 0;

    /// @brief Notify that a data packet has been received.
    /// @param handle Handle to the receiver instance for which sources were found.
    /// @param source_addr IP address & port of the packet source.
    /// @param header The sACN header data.
    /// @param pdata The DMX data.  Use header.slot_count to determine the length of this array.
    virtual void HandleUniverseData(Handle handle, const etcpal::SockAddr& source_addr, const SacnHeaderData& header,
                              const uint8_t* pdata) = 0;

    /// @brief Notify that one or more sources have entered a data loss state.
    /// @param handle Handle to the receiver instance for which sources were lost.
    /// @param lost_sources Array of structs describing the source or sources that have been lost.
    /// @param num_lost_sources Size of the lost_sources array.
    /// @param context Context pointer that was given at the creation of the receiver instance.
    virtual void HandleSourcesLost(Handle handle, const SacnLostSource* lost_sources, size_t num_lost_sources) = 0;

    /// @brief Notify that a source has stopped transmission of per-address priority packets.
    /// @param handle Handle to the receiver instance for which a source stopped sending per-address priority.
    /// @param source Information about the source that has stopped transmission of per-address priority.
    virtual void HandleSourcePapLost(Handle handle, const SacnRemoteSource& source) = 0;

    /// @brief Notify that more than the configured maximum number of sources are currently sending on the universe
    /// being listened to.
    /// @param handle Handle to the receiver instance for which the source limit has been exceeded.
    virtual void HandleSourceLimitExceeded(Handle handle) = 0;
  };

  /// @ingroup sacn_receiver_cpp
  /// @brief A set of configuration settings that a receiver needs to initialize.
  struct Settings
  {
    /********* Required values **********/

    uint16_t universe_id{0};  ///< The sACN universe number the receiver is listening to.

    /********* Optional values **********/

    size_t source_count_max{SACN_RECEIVER_INFINITE_SOURCES};  ///< The maximum number of sources this universe will
                                                              ///< listen to when using dynamic memory.
    unsigned int flags{0};                   ///< A set of option flags. See the C API's "sACN receiver flags".
    std::vector<SacnMcastNetintId> netints;  ///< If non-empty, the list of network interfaces to listen on.  Otherwise
                                             ///< all available interfaces are used.

    /// Create an empty, invalid data structure by default.
    Settings() = default;
    Settings(uint16_t universe_id);

    bool IsValid() const;
  };

  Receiver() = default;
  Receiver(const Receiver& other) = delete;
  Receiver& operator=(const Receiver& other) = delete;
  Receiver(Receiver&& other) = default;             ///< Move a device instance.
  Receiver& operator=(Receiver&& other) = default;  ///< Move a device instance.

  etcpal::Error Startup(NotifyHandler& notify_handler, const Settings& settings);
  void Shutdown();
  etcpal::Error ChangeUniverse(uint16_t new_universe_id);
  etcpal::Error ResetNetworking(const std::vector<SacnMcastNetintId>& netints);

  // Lesser used functions
  void SetStandardVersion(sacn_standard_version_t version);
  sacn_standard_version_t GetStandardVersion() const;
  void SetExpiredWait(uint32_t wait_ms);
  uint32_t GetExpiredWait() const;

  constexpr Handle handle() const;
  constexpr NotifyHandler* notify_handler() const;
  etcpal::Expected<uint16_t> universe() const;

private:
  SacnReceiverConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_{kInvalidHandle};
  NotifyHandler* notify_{nullptr};
};

/// @cond device_c_callbacks
/// Callbacks from underlying device library to be forwarded
namespace internal
{
extern "C" inline void ReceiverCbSourcesFound(sacn_receiver_t handle, const SacnFoundSource* found_sources,
                                              size_t num_found_sources, void* context)
{
  if (context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourcesFound(handle, found_sources, num_found_sources);
  }
}

extern "C" inline void ReceiverCbUniverseData(sacn_receiver_t handle, const EtcPalSockAddr* source_addr,
                                         const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  if (source_addr && header && context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleUniverseData(handle, etcpal::SockAddr{*source_addr}, *header,
                                                                       pdata);
  }
}

extern "C" inline void ReceiverCbSourcesLost(sacn_receiver_t handle, const SacnLostSource* lost_sources,
                                        size_t num_lost_sources, void* context)
{
  if (context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourcesLost(handle, lost_sources, num_lost_sources);
  }
}

extern "C" inline void ReceiverCbPapLost(sacn_receiver_t handle, const SacnRemoteSource* source, void* context)
{
  if (source && context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourcePapLost(handle, *source);
  }
}

extern "C" inline void ReceiverCbSourceLimitExceeded(sacn_receiver_t handle, void* context)
{
  if (context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourceLimitExceeded(handle);
  }
}

};  // namespace internal

/// @endcond


};  // namespace sacn

#endif  // SACN_CPP_RECEIVER_H_
