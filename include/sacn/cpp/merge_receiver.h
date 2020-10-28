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

/**
 * @file sacn/cpp/merge_receiver.h
 * @brief C++ wrapper for the sACN Merge Receiver API
 */

#include "sacn/merge_receiver.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"

/**
 * @defgroup sacn_merge_receiver_cpp sACN Merge Receiver API
 * @ingroup sacn_cpp_api
 * @brief A C++ wrapper for the sACN Merge Receiver API
 */

namespace sacn
{
/**
 * @ingroup sacn_merge_receiver_cpp
 * @brief An instance of sACN Merge Receiver functionality; see @ref using_merge_receiver.
 *
 * This API is used to minimally wrap the sACN Receiver and DMX Merger logic together so an application can receive and
 * merge sACN sources in software.
 *
 * See @ref using_merge_receiver for a detailed description of how to use this API.
 */
class MergeReceiver
{
public:
  /** A handle type used by the sACN library to identify merge receiver instances. */
  using Handle = sacn_merge_receiver_t;
  /** An invalid Handle value. */
  static constexpr Handle kInvalidHandle = SACN_MERGE_RECEIVER_INVALID;

  /**
   * @ingroup sacn_merge_receiver_cpp
   * @brief A base class for a class that receives notification callbacks from a sACN merge receiver.
   */
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /**
     * @brief Notify that a new data packet has been received and merged.
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
     * @param[in] universe The universe this merge receiver is monitoring.
     * @param[in] slots Buffer of #DMX_ADDRESS_COUNT bytes containing the merged levels for the universe.  This buffer
     *                  is owned by the library.
     * @param[in] slot_owners Buffer of #DMX_ADDRESS_COUNT source_ids.  If a value in the buffer is
     *           #DMX_MERGER_SOURCE_INVALID, the corresponding slot is not currently controlled. You can also use
     *            SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners, index) to check the slot validity. This buffer is owned
     *            by the library.
     */
    virtual void HandleMergedData(uint16_t universe, const uint8_t* slots, const sacn_source_id_t* slot_owners) = 0;

    /**
     * @brief Notify that a non-data packet has been received.
     *
     * When an established source sends a sACN data packet that doesn't contain DMX values or priorities, the raw packet
     * is immediately and synchronously passed to this callback.
     *
     * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
     * packets on the universe.
     *
     * @param[in] universe The universe this merge receiver is monitoring.
     * @param[in] source_addr The network address from which the sACN packet originated.
     * @param[in] header The header data of the sACN packet.
     * @param[in] pdata Pointer to the data buffer. Size of the buffer is indicated by header->slot_count. This buffer
     *                  is owned by the library.
     */
    virtual void HandleNonDmxData(uint16_t universe, const etcpal::SockAddr& source_addr,
                                  const SacnHeaderData& header, const uint8_t* pdata) = 0;

    /**
     * @brief Notify that more than the configured maximum number of sources are currently sending on
     *        the universe being listened to.
     *
     * This is a notification that is directly forwarded from the sACN Receiver module.
     *
     * @param[in] universe The universe this merge receiver is monitoring.
     */
    virtual void HandleSourceLimitExceeded(uint16_t universe) = 0;
  };

  /**
   * @ingroup sacn_merge_receiver_cpp
   * @brief A set of configuration settings that a merge receiver needs to initialize.
   */
  struct Settings
  {
    /********* Required values **********/

    /** The sACN universe number the merge receiver is listening to. */
    uint16_t universe_id{0};

    /********* Optional values **********/

    /** The maximum number of sources this universe will listen to when using dynamic memory. */
    size_t source_count_max{SACN_RECEIVER_INFINITE_SOURCES};
    /// If non-empty, the list of network interfaces to listen on.  Otherwise all available interfaces are used.
    std::vector<EtcPalMcastNetintId> netints; 

    /** Create an empty, invalid data structure by default. */
    Settings() = default;
    Settings(uint16_t new_universe_id);

    bool IsValid() const;
  };

  MergeReceiver() = default;
  MergeReceiver(const MergeReceiver& other) = delete;
  MergeReceiver& operator=(const MergeReceiver& other) = delete;
  MergeReceiver(MergeReceiver&& other) = default;             /**< Move a merge receiver instance. */
  MergeReceiver& operator=(MergeReceiver&& other) = default;  /**< Move a merge receiver instance. */

  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler,
                        std::vector<SacnMcastInterface>& netints);
  void Shutdown();
  etcpal::Expected<uint16_t> GetUniverse() const;
  etcpal::Error ChangeUniverse(uint16_t new_universe_id);
  etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints);

  etcpal::Expected<sacn_source_id_t> GetSourceId(const etcpal::Uuid& source_cid) const;
  etcpal::Expected<etcpal::Uuid> GetSourceCid(sacn_source_id_t source) const;

  constexpr Handle handle() const;

private:
  SacnMergeReceiverConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_{kInvalidHandle};
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void MergeReceiverCbMergedData(sacn_merge_receiver_t handle, uint16_t universe, const uint8_t* slots,
                                                 const sacn_source_id_t* slot_owners, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleMergedData(universe, slots, slot_owners);
  }
}

extern "C" inline void MergeReceiverCbNonDmx(sacn_merge_receiver_t handle, uint16_t universe, const EtcPalSockAddr* source_addr,
                                             const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context && source_addr && header)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleNonDmxData(universe, *source_addr, *header, pdata);
  }
}

extern "C" inline void MergeReceiverCbSourceLimitExceeded(sacn_merge_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSourceLimitExceeded(universe);
  }
}

};  // namespace internal

/**
 * @endcond
 */

/**
 * @brief Create a MergeReceiver Settings instance by passing the required members explicitly.
 *
 * Optional members can be modified directly in the struct.
 */
inline MergeReceiver::Settings::Settings(uint16_t new_universe_id) : universe_id(new_universe_id)
{
}

/** Determine whether a MergeReciever Settings instance contains valid data for sACN operation. */
inline bool MergeReceiver::Settings::IsValid() const
{
  return (universe_id > 0);
}

/**
 * @brief Start listening for sACN data on a universe.
 *
 * An sACN merge receiver can listen on one universe at a time, and each universe can only be listened to
 * by one merge receiver at at time.
 *
 * Note that a merge receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN merge receiver and this class instance.
 * @param[in] notify_handler The notification interface to call back to the application.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Merge Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this merge receiver, or maximum merge receivers reached.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                            std::vector<SacnMcastInterface>& netints)
{
  SacnMergeReceiverConfig config = TranslateConfig(settings, notify_handler);

  if(netints.empty())
    return sacn_merge_receiver_create(&config, &handle_, NULL, 0);
  
  return sacn_merge_receiver_create(&config, &handle_, netints.data(), netints.size());
}

/**
 * @brief Stop listening for sACN data on a universe.
 *
 * Tears down the merge receiver and any sources currently being tracked on the merge receiver's universe.
 * Stops listening for sACN on that universe.
 */
inline void MergeReceiver::Shutdown()
{
  sacn_merge_receiver_destroy(handle_);
  handle_ = kInvalidHandle;
}

/**
 * @brief Get the universe this class is listening to.
 *
 * @return If valid, the value is the universe id.  Otherwise, this is the underlying error the C library call returned.
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

/**
 * @brief Change the universe this class is listening to.
 *
 * An sACN merge receiver can only listen on one universe at a time. After this call completes successfully, the merge
 * receiver is in a sampling period for the new universe and will provide HandleSourcesFound() calls when appropriate.
 * If this call fails, the caller must call Shutdown() on this class, because it may be in an invalid state.
 *
 * @param[in] new_universe_id New universe number that this merge receiver should listen to.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified new universe.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::ChangeUniverse(uint16_t new_universe_id)
{
  return sacn_merge_receiver_change_universe(handle_, new_universe_id);
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for this class..
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the merge receiver is in a sampling period for the new universe and will provide
 * HandleSourcesFound() calls when appropriate.
 * If this call fails, the caller must call Shutdown() on this class, because it may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merge receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::ResetNetworking(std::vector<SacnMcastInterface>& netints)
{
  if (netints.empty())
    return sacn_merge_receiver_reset_networking(handle_, nullptr, 0);

  return sacn_merge_receiver_reset_networking(handle_, netints.data(), netints.size());
}

/**
 * @brief Returns the source id for that source cid.
 *
 * @param[in] source_cid The UUID of the source CID.
 * @return On success this will be the source ID, otherwise kEtcPalErrInvalid.
 */
inline etcpal::Expected<sacn_source_id_t> MergeReceiver::GetSourceId(const etcpal::Uuid& source_cid) const
{
  sacn_source_id_t result = sacn_merge_receiver_get_source_id(handle_, &source_cid.get());
  if (result != SACN_DMX_MERGER_SOURCE_INVALID)
    return result;
  return kEtcPalErrInvalid;
}

/**
 * @brief Returns the source cid for that source id.
 *
 * @param[in] source_id The id of the source.
 * @return On success this will be the source CID, otherwise kEtcPalErrInvalid.
 */
inline etcpal::Expected<etcpal::Uuid> MergeReceiver::GetSourceCid(sacn_source_id_t source_id) const
{
  EtcPalUuid result;
  if (kEtcPalErrOk == sacn_merge_receiver_get_source_cid(handle_, source_id, &result))
    return result;
  return kEtcPalErrInvalid;
}

/**
 * @brief Get the current handle to the underlying C merge receiver.
 *
 * @return The handle or Receiver::kInvalidHandle.
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
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_MERGE_RECEIVER_H_
