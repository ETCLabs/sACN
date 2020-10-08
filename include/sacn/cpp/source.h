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

#ifndef SACN_CPP_SOURCE_H_
#define SACN_CPP_SOURCE_H_

/// @file sacn/cpp/source.h
/// @brief C++ wrapper for the sACN Source API

#include "sacn/source.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/inet.h"

/// @defgroup sacn_source_cpp sACN Source API
/// @ingroup sacn_cpp_api
/// @brief A C++ wrapper for the sACN Source API

namespace sacn
{
/// @ingroup sacn_source_cpp
/// @brief An instance of sACN Source functionality.
///
/// CHRISTIAN TODO: FILL OUT THIS COMMENT MORE -- DO WE NEED A using_source.md???
class Source
{
public:
  /// A handle type used by the sACN library to identify source instances.
  using Handle = sacn_source_t;
  /// An invalid Handle value.
  static constexpr Handle kInvalidHandle = SACN_SOURCE_INVALID;

  /// @ingroup sacn_source_cpp
  /// @brief A set of configuration settings that a source needs to initialize.
  struct Settings
  {
  /********* Required values **********/

  /*! The source's CID. */
  Uuid cid;
  /*! The source's name, a UTF-8 encoded string. Up to #SACN_SOURCE_NAME_MAX_LEN characters will be used. */
  std::string name;

  /********* Optional values **********/

  /*! The maximum number of sources this universe will send to when using dynamic memory. */
  size_t universe_count_max{SACN_SOURCE_INFINITE_UNIVERSES};

  /*! If non-empty, the list of network interfaces to transmit on.  Otherwise, all available interfaces are used. */
  std::vector<SacnMcastNetintId> netints;

  /*! If false (default), this module starts a thread that calls sacn_source_process_sources() every 23 ms.
      If true, no thread is started and the application must call sacn_source_process_sources() at its DMX rate,
      usually 23 ms. */
  bool manually_process_source;

    /// Create an empty, invalid data structure by default.
    Settings() = default;
    Settings(const Uuid& new_cid, const std::string& new_name);

    bool IsValid() const;
  };

  Source() = default;
  Source(const Source& other) = delete;
  Source& operator=(const Source& other) = delete;
  Source(Source&& other) = default;             ///< Move a device instance.
  Source& operator=(Source&& other) = default;  ///< Move a device instance.

  etcpal::Error Startup(const Settings& settings, SacnNetworkChangeResult* good_interfaces = nullptr);
  void Shutdown();

  etcpal::Error ChangeName(const std::string& new_name);

  etcpal::Error AddUniverse(const SacnSourceUniverseConfig& config, bool dirty_now);
  void RemoveUniverse(uint16_t universe);
  etcpal::Error AddUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest, bool dirty_now);
  void RemoveUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest);

  etcpal::Expected<uint16_t> GetUniverse() const;
  etcpal::Error ChangeUniverse(uint16_t new_universe_id);
  etcpal::Error ResetNetworking(const std::vector<SacnMcastNetintId>& netints,
                                SacnNetworkChangeResult* good_interfaces = nullptr);


etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint16_t universe, uint8_t new_priority);
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, uint16_t universe, bool new_preview_flag);
etcpal_error_t sacn_source_change_synchronization_universe(sacn_source_t handle, uint16_t universe,
                                                           uint16_t new_sync_universe);

etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen);
etcpal_error_t sacn_source_send_synchronization(sacn_source_t handle, uint16_t universe);

void sacn_source_set_dirty(sacn_source_t handle, uint8_t universe);
void sacn_source_set_list_dirty(sacn_source_t handle, uint8_t* universes, size_t num_universes);
void sacn_source_set_dirty_and_force_sync(sacn_source_t handle, uint8_t universe);

size_t sacn_source_process_sources(void);

etcpal_error_t sacn_source_reset_networking(sacn_source_t handle, const SacnMcastNetintId* netints, size_t num_netints, SacnNetworkChangeResult* good_interfaces);

  constexpr Handle handle() const;

private:
  SacnSourceConfig TranslateConfig(const Settings& settings);

  Handle handle_{kInvalidHandle};
};

/// @brief Create a Receiver Settings instance by passing the required members explicitly.
///
/// Optional members can be modified directly in the struct.
inline Receiver::Settings::Settings(uint16_t new_universe_id) : universe_id(new_universe_id)
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
 * Note that a receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces listed in the passed in configuration.  This will only return #kEtcPalErrNoNetints
 * if none of the interfaces work.
 *
 * \param[in] settings Configuration parameters for the sACN receiver and this class instance.
 * \param[in] notify_handler The notification interface to call back to the application.
 * \param[in, out] good_interfaces Optional. If non-nil, good_interfaces is filled in with the list of network
 * interfaces that were succesfully used.
 * \return #kEtcPalErrOk: Receiver created successfully.
 * \return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * \return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                       SacnNetworkChangeResult* good_interfaces)
{
  SacnReceiverConfig config = TranslateConfig(settings, notify_handler);
  return sacn_receiver_create(&config, &handle_, good_interfaces);
}

/*!
 * \brief Stop listening for sACN data on a universe.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 */
inline void Receiver::Shutdown()
{
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
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * \param[in] netints Vector of network interfaces on which to listen to the specified universe. If empty,
 *  all available network interfaces will be used.
 * \param[in, out] good_interfaces Optional. If non-nil, good_interfaces is filled in with the list of network
 * interfaces that were succesfully used.
 * \return #kEtcPalErrOk: Universe changed successfully.
 * \return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::ResetNetworking(const std::vector<SacnMcastNetintId>& netints,
                                               SacnNetworkChangeResult* good_interfaces)
{
  if (netints.empty())
    return sacn_receiver_reset_networking(handle_, nullptr, 0, good_interfaces);
  else
    return sacn_receiver_reset_networking(handle_, netints.data(), netints.size(), good_interfaces);
}

/*!
 * \brief Get the current handle to the underlying C sacn_source.
 *
 * \return The handle or Source::kInvalidHandle.
 */
inline constexpr Source::Handle Source::handle() const
{
  return handle_;
}

inline SacnSourceConfig Source::TranslateConfig(const Settings& settings, NotifyHandler& notify_handler)
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

  // Now initialize the netints
  if (config.num_netints > 0)
  {
    config.netints = settings.netints.data();
  }

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_SOURCE_H_
