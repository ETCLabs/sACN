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

#ifndef SACN_CPP_MERGE_RECEIVER_H_
#define SACN_CPP_MERGE_RECEIVER_H_

/// @file sacn/cpp/merge_receiver.h
/// @brief C++ wrapper for the sACN Merge Receiver API

#include "sacn/merge_receiver.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"

/// @defgroup sacn_merge_receiver_cpp sACN Merge Receiver API
/// @ingroup sacn_cpp_api
/// @brief A C++ wrapper for the sACN Merge Receiver API

namespace sacn
{
/// @ingroup sacn_merge_receiver_cpp
/// @brief An instance of sACN Merge Receiver functionality.
///
/// TODO: FILL OUT THIS COMMENT MORE -- DO WE NEED A using_merge_receiver.md???
class MergeReceiver
{
public:
  /// A handle type used by the sACN library to identify receiver instances.
  using Handle = sacn_merge_receiver_t;
  /// An invalid Handle value.
  static constexpr Handle kInvalidHandle = SACN_MERGE_RECEIVER_INVALID;

  /// @ingroup sacn_merge_receiver_cpp
  /// @brief A base class for a class that receives notification callbacks from a sACN merge receiver.
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /*!
     * \brief Notify that a new data packet has been received and merged.
     *
     * This callback will be called in multiple ways:
     * 1. When a new non-preview data packet or per-address priority packet is received from the sACN Receiver module,
     * it is immediately and synchronously passed to the DMX Merger, after which the merged result is immediately and
     * synchronously passed to this callback.  Note that this includes the data received from the
     * SacnSourcesFoundCallback().
     * 2. When a sACN source is no longer sending non-preview data or per-address priority packets, the lost source
     * callback from the sACN Receiver module will be passed to the merger, after which the merged result is immediately
     * and synchronously passed to this callback.
     *
     * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
     * packets on the universe.
     *
     * \param[in] universe The universe number this receiver is monitoring.
     * \param[in] slots Buffer of #DMX_ADDRESS_COUNT bytes containing the merged levels for the universe.  This buffer
     *                  is owned by the library.
     * \param[in] slot_owners Buffer of #DMX_ADDRESS_COUNT source_ids.  If a value in the buffer is
     *           #DMX_MERGER_SOURCE_INVALID, the corresponding slot is not currently controlled. You can also use
     *            SACN_DMX_MERGER_IS_SOURCE_VALID(slot_owners[index]) to check the slot validity. This buffer is owned
     *            by the library.
     */
    virtual void HandleMergedData(uint16_t universe, const uint8_t* slots, const source_id_t* slot_owners) = 0;

    /*!
     * \brief Notify that a non-data packet has been received.
     *
     * When an established source sends a sACN data packet that doesn't contain DMX values or priorities, the raw packet
     * is immediately and synchronously passed to this callback.
     *
     * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
     * packets on the universe.
     *
     * If the source is sending sACN Sync packets, this callback will only be called when the sync packet is received,
     * if the source forces the packet, or if the source sends a data packet without a sync universe.
     *
     * \param[in] universe The universe number this receiver is monitoring.
     * \param[in] source_addr The network address from which the sACN packet originated.
     * \param[in] header The header data of the sACN packet.
     * \param[in] pdata Pointer to the data buffer. Size of the buffer is indicated by header->slot_count. This buffer
     *                  is owned by the library.
     */
    virtual void HandleNonDMXPacket(uint16_t universe, const etcpal::SockAddr& source_addr,
                                    const SacnHeaderData& header, const uint8_t* pdata) = 0;

    /*!
     * \brief Notify that more than the configured maximum number of sources are currently sending on
     *        the universe being listened to.
     *
     * This is a notification that is directly forwarded from the sACN Receiver module.
     *
     * \param[in] handle Handle to the receiver instance for which the source limit has been exceeded.
     */
    virtual void HandleSourceLimitExceeded(uint16_t universe) = 0;
  };

  /// @ingroup sacn_merge_receiver_cpp
  /// @brief A set of configuration settings that a receiver needs to initialize.
  struct Settings
  {
    /********* Required values **********/

    uint16_t universe_id{0};  ///< The sACN universe number the receiver is listening to.

    /********* Optional values **********/

    size_t source_count_max{SACN_RECEIVER_INFINITE_SOURCES};  ///< The maximum number of sources this universe will
                                                              ///< listen to when using dynamic memory.
    std::vector<SacnMcastNetintId> netints;  ///< If non-empty, the list of network interfaces to listen on.  Otherwise
                                             ///< all available interfaces are used.

    /// Create an empty, invalid data structure by default.
    Settings() = default;
    Settings(uint16_t new_universe_id);

    bool IsValid() const;
  };

  MergeReceiver() = default;
  MergeReceiver(const MergeReceiver& other) = delete;
  MergeReceiver& operator=(const MergeReceiver& other) = delete;
  MergeReceiver(MergeReceiver&& other) = default;             ///< Move a device instance.
  MergeReceiver& operator=(MergeReceiver&& other) = default;  ///< Move a device instance.

  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler);
  void Shutdown();
  etcpal::Expected<uint16_t> GetUniverse() const;
  etcpal::Error ChangeUniverse(uint16_t new_universe_id);
  etcpal::Error ResetNetworking(const std::vector<SacnMcastNetintId>& netints);

  etcpal::Expected<source_id_t> GetSourceId(const etcpal::Uuid& source_cid) const;
  etcpal::Expected<etcpal::Uuid> GetSourceCid(source_id_t source) const;

  constexpr Handle handle() const;

private:
  SacnMergeReceiverConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_{kInvalidHandle};
};

/// @cond device_c_callbacks
/// Callbacks from underlying device library to be forwarded
namespace internal
{
extern "C" inline void MergeReceiverCbMergedData(uint16_t universe, const uint8_t* slots,
                                                 const source_id_t* slot_owners, void* context)
{
  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleMergedData(universe, slots, slot_owners);
  }
}

extern "C" inline void MergeReceiverCbNonDmx(uint16_t universe, const EtcPalSockAddr* source_addr,
                                             const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  if (context && source_addr && header)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleNonDMXPacket(universe, *source_addr, *header, pdata);
  }
}

extern "C" inline void MergeReceiverCbSourceLimitExceeded(uint16_t universe, void* context)
{
  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSourceLimitExceeded(universe);
  }
}

};  // namespace internal

/// @endcond

/// @brief Create a MergeReceiver Settings instance by passing the required members explicitly.
///
/// Optional members can be modified directly in the struct.
inline MergeReceiver::Settings::Settings(uint16_t new_universe_id) : universe_id(new_universe_id)
{
}

/// Determine whether a MergeReciever Settings instance contains valid data for sACN operation.
inline bool MergeReceiver::Settings::IsValid() const
{
  return (universe_id > 0);
}

/*!
 * \brief Start listening for sACN data on a universe.
 *
 * An sACN merge receiver can listen on one universe at a time, and each universe can only be listened to
 * by one receiver at at time.
 *
 * \param[in] settings Configuration parameters for the sACN merge receiver and this class instance.
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
inline etcpal::Error MergeReceiver::Startup(const Settings& settings, NotifyHandler& notify_handler)
{
  SacnMergeReceiverConfig config = TranslateConfig(settings, notify_handler);
  return sacn_merge_receiver_create(&config, &handle_);
}

/*!
 * \brief Stop listening for sACN data on a universe.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 */
inline void MergeReceiver::Shutdown()
{
  // We'll be ignoring shutdown errors for now
  sacn_merge_receiver_destroy(handle_);
  handle_ = kInvalidHandle;
}

/*!
 * \brief Get the universe this class is listening to.
 *
 * \return If valid, the value is the universe id.  Otherwise, this is the underlying error the C library call returned.
 */
etcpal::Expected<uint16_t> MergeReceiver::GetUniverse() const
{
  uint16_t result = 0;
  etcpal_error_t err = sacn_merge_receiver_get_universe(handle_, &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/*!
 * \brief Change the universe this class is listening to.
 *
 * An sACN receiver can only listen on one universe at a time. After this call completes successfully, the receiver is
 * in a sampling period for the new universe and will provide HandleSourcesFound() callse when appropriate.
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
inline etcpal::Error MergeReceiver::ChangeUniverse(uint16_t new_universe_id)
{
  return sacn_merge_receiver_change_universe(handle_, new_universe_id);
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
 * \param[in] netints Vectorof network interfaces on which to listen to the specified universe. If empty,
 *  all available network interfaces will be used.
 * \return #kEtcPalErrOk: Universe changed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::ResetNetworking(const std::vector<SacnMcastNetintId>& netints)
{
  if (netints.empty())
    return sacn_merge_receiver_reset_networking(handle_, nullptr, 0);
  else
    return sacn_merge_receiver_reset_networking(handle_, netints.data(), netints.size());
}

/*!
 * \brief Returns the source id for that source cid.
 *
 * \param[in] source_cid The UUID of the source CID.
 * \return On success this will be the source ID, otherwise kEtcPalErrInvalid.
 */
inline etcpal::Expected<source_id_t> MergeReceiver::GetSourceId(const etcpal::Uuid& source_cid) const
{
  source_id_t result = sacn_merge_receiver_get_source_id(handle_, &source_cid.get());
  if (result != SACN_DMX_MERGER_SOURCE_INVALID)
    return result;
  return kEtcPalErrInvalid;
}

/*!
 * \brief Returns the source cid for that source id.
 *
 * Looks up the source data and returns a pointer to the data or nullptr if it doesn't exist.
 * This pointer is owned by the library, and must not be modified by the application.
 * The pointer will only be valid until the source or merger is removed.
 *
 * \param[in] source_id The id of the source.
 * \return On success this will be the source CID, otherwise kEtcPalErrInvalid.
 */
inline etcpal::Expected<etcpal::Uuid> MergeReceiver::GetSourceCid(source_id_t source_id) const
{
  EtcPalUuid result;
  if (kEtcPalErrOk == sacn_merge_receiver_get_source_cid(handle_, source_id, &result))
    return result;
  return kEtcPalErrInvalid;
}

/*!
 * \brief Get the current handle to the underlying C sacn_receiver.
 *
 * \return The handle or Receiver::kInvalidHandle.
 */
inline constexpr MergeReceiver::Handle MergeReceiver::handle() const
{
  return handle_;
}

inline SacnMergeReceiverConfig MergeReceiver::TranslateConfig(const Settings& settings, NotifyHandler& notify_handler)
{
  // clang-format off
  SacnMergeReceiverConfig config = {
    settings.universe_id,
    {
      internal::MergeReceiverCbMergedData,
      internal::MergeReceiverCbNonDmx,
      internal::MergeReceiverCbSourceLimitExceeded,
      &notify_handler
    },
    settings.source_count_max,
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

#endif  // SACN_CPP_MERGE_RECEIVER_H_
