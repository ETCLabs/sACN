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
/// CHRISTIAN TODO: FILL OUT THIS COMMENT MORE -- DO WE NEED A using_receiver.md???
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
    Settings(uint16_t new_universe_id);

    bool IsValid() const;
  };

  Receiver() = default;
  Receiver(const Receiver& other) = delete;
  Receiver& operator=(const Receiver& other) = delete;
  Receiver(Receiver&& other) = default;             ///< Move a device instance.
  Receiver& operator=(Receiver&& other) = default;  ///< Move a device instance.

  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler);
  void Shutdown();
  etcpal::Expected<uint16_t> GetUniverse() const;
  etcpal::Error ChangeUniverse(uint16_t new_universe_id);
  etcpal::Error ResetNetworking(const std::vector<SacnMcastNetintId>& netints);

  // Lesser used functions.  These apply to all instances of this class.
  static void SetStandardVersion(sacn_standard_version_t version);
  static sacn_standard_version_t GetStandardVersion();
  static void SetExpiredWait(uint32_t wait_ms);
  static uint32_t GetExpiredWait();

  constexpr Handle handle() const;

private:
  SacnReceiverConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_{kInvalidHandle};
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
    static_cast<Receiver::NotifyHandler*>(context)->HandleUniverseData(handle, *source_addr, *header,
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

/// @brief Create a Receiver Settings instance by passing the required members explicitly.
///
/// Optional members can be modified directly in the struct.
inline Receiver::Settings::Settings(uint16_t new_universe_id)
    : universe_id(new_universe_id)
{
}

/// Determine whether a Reciever Settings instance contains valid data for sACN operation.
inline bool Receiver::Settings::IsValid() const
{
  return (universe_id > 0);
}

/*!
 * \brief Start listening for sACN data on a universe.
 *
 * An sACN receiver can listen on one universe at a time, and each universe can only be listened to
 * by one receiver at at time.
 *
 * \param[in] settings Configuration parameters for the sACN receiver and this class instance.
 * \param[in] notify_handler The notification interface to call back to the application.
 * \return #kEtcPalErrOk: Receiver created successful.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * \return #kEtcPalErrNoNetints: No network interfaces were found on the system.
 * \return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::Startup(const Settings& settings, NotifyHandler& notify_handler)
{
  SacnReceiverConfig config = TranslateConfig(settings, notify_handler);
  return sacn_receiver_create(&config, &handle_);
}

/*!
 * \brief Stop listening for sACN data on a universe.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 */
inline void Receiver::Shutdown()
{
  //We'll be ignoring shutdown errors for now
  sacn_receiver_destroy(handle_);
  handle_ = kInvalidHandle;
}

/*!
 * \brief Get the universe this class is listening to.
 *
 * \return If valid, the value is the universe id.  Otherwise, this is the underlying error the C library call returned.
 */
etcpal::Expected<uint16_t> Receiver::GetUniverse() const
{
  uint16_t result = 0;
  etcpal_error_t err = sacn_receiver_get_universe(handle_, &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/*!
 * \brief Change the universe this class is listening to.
 *
 * An sACN receiver can only listen on one universe at a time. After this call completes successfully, the receiver is
 * in a sampling period for the new universe and will provide HandleSourcesFound() calls when appropriate.
 * If this call fails, the caller must call Shutdown() on this class, because it may be in an invalid state.
 *
 * \param[in] new_universe_id New universe number that this receiver should listen to.
 * \return #kEtcPalErrOk: Universe changed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: A receiver already exists which is listening on the specified new universe.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::ChangeUniverse(uint16_t new_universe_id)
{
  return sacn_receiver_change_universe(handle_, new_universe_id);
}

/*!
 * \brief Resets the underlying network sockets and packet receipt state for this class..
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the receiver is in a sampling period for the new universe and will provide
 * HandleSourcesFound() calls when appropriate.
 * If this call fails, the caller must call Shutdown() on this class, because it may be in an invalid state.
 *
 * \param[in] netints Vector of network interfaces on which to listen to the specified universe. If empty,
 *  all available network interfaces will be used.
 * \return #kEtcPalErrOk: Universe changed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::ResetNetworking(const std::vector<SacnMcastNetintId>& netints)
{
  if (netints.empty())
    return sacn_receiver_reset_networking(handle_, nullptr, 0);
  else
    return sacn_receiver_reset_networking(handle_, netints.data(), netints.size());
}

/*!
 * \brief Set the current version of the sACN standard to which the module is listening.
 *
 * This is a global option across all listening receivers.
 *
 * \param[in] version Version of sACN to listen to.
 */
inline void Receiver::SetStandardVersion(sacn_standard_version_t version)
{
  sacn_receiver_set_standard_version(version);
}

/*!
 * \brief Get the current version of the sACN standard to which the module is listening.
 *
 * This is a global option across all listening receivers.
 *
 * \return Version of sACN to which the module is listening, or #kSacnStandardVersionNone if the module is
 *         not initialized.
 */
inline sacn_standard_version_t Receiver::GetStandardVersion()
{
  sacn_receiver_get_standard_version();
}

/*!
 * \brief Set the expired notification wait time.
 *
 * The library will wait at least this long after a data loss condition has been encountered before
 * calling HandleSourcesLost(). However, the wait may be longer due to the data loss algorithm (see \ref
 * data_loss_behavior).
 *
 * \param[in] wait_ms Wait time in milliseconds.
 */
inline void Receiver::SetExpiredWait(uint32_t wait_ms)
{
  sacn_receiver_set_expired_wait(wait_ms);
}

/*!
 * \brief Get the current value of the expired notification wait time.
 *
 * The library will wait at least this long after a data loss condition has been encountered before
 * calling HandleSourcesLost(). However, the wait may be longer due to the data loss algorithm (see \ref
 * data_loss_behavior).
 *
 * \return Wait time in milliseconds.
 */
inline uint32_t Receiver::GetExpiredWait()
{
  return sacn_receiver_get_expired_wait();
}

/*!
 * \brief Get the current handle to the underlying C sacn_receiver.
 *
 * \return The handle or Receiver::kInvalidHandle.
 */
inline constexpr Receiver::Handle Receiver::handle() const
{
  return handle_;
}

inline SacnReceiverConfig Receiver::TranslateConfig(const Settings& settings, NotifyHandler& notify_handler)
{
  // clang-format off
  SacnReceiverConfig config = {
    settings.universe_id,
    {
      internal::ReceiverCbSourcesFound,
      internal::ReceiverCbUniverseData,
      internal::ReceiverCbSourcesLost,
      internal::ReceiverCbPapLost,
      internal::ReceiverCbSourceLimitExceeded,
      &notify_handler
    },
    settings.source_count_max,
    settings.flags,
    nullptr, 
    settings.netints.size()
  };
  // clang-format on

  //Now initialize the netints
  if (config.num_netints > 0)
  {
    config.netints = settings.netints.data();
  }

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_RECEIVER_H_
