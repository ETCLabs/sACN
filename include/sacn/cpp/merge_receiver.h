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

#ifndef SACN_CPP_MERGE_RECEIVER_H_
#define SACN_CPP_MERGE_RECEIVER_H_

/**
 * @file sacn/cpp/merge_receiver.h
 * @brief C++ wrapper for the sACN Merge Receiver API
 */

#include "sacn/cpp/common.h"

#include "sacn/merge_receiver.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/opaque_id.h"
#include "etcpal/cpp/uuid.h"

/**
 * @defgroup sacn_merge_receiver_cpp sACN Merge Receiver API
 * @ingroup sacn_cpp_api
 * @brief A C++ wrapper for the sACN Merge Receiver API
 */

namespace sacn
{

namespace detail
{
class MergeReceiverHandleType
{
};
};  // namespace detail

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
  using Handle = etcpal::OpaqueId<detail::MergeReceiverHandleType, sacn_merge_receiver_t, SACN_MERGE_RECEIVER_INVALID>;

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
     * it is immediately and synchronously passed to a DMX Merger. If the sampling period has not ended for the
     * source, the merged result is not passed to this callback until the sampling period ends. Otherwise, it is
     * immediately and synchronously passed to this callback.
     * 2. When a sACN source is no longer sending non-preview data or per-address priority packets, the lost source
     * callback from the sACN Receiver module will be passed to a merger, after which the merged result is passed to
     * this callback pending the sampling period.
     *
     * After a networking reset, some of the sources on the universe may not be included in the resulting sampling
     * period. Therefore, expect this to continue to be called during said sampling period.
     *
     * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
     * packets on the universe.
     *
     * @param[in] handle The merge receiver's handle.
     * @param[in] merged_data The merged data (and relevant information about that data), starting from the first slot
     * of the currently configured footprint. Only sources that are not currently part of a sampling period are part of
     * the merged result.
     */
    virtual void HandleMergedData(Handle handle, const SacnRecvMergedData& merged_data) = 0;

    /**
     * @brief Notify that a non-data packet has been received.
     *
     * When an established source sends a sACN data packet that doesn't contain DMX values or priorities, the raw data
     * within the configured footprint is immediately and synchronously passed to this callback.
     *
     * This callback should be processed quickly, since it will interfere with the receipt and processing of other sACN
     * packets on the universe.
     *
     * If the source is sending sACN Sync packets, this callback will only be called when the sync packet is received,
     * if the source forces the packet, or if the source sends a data packet without a sync universe.
     * TODO: this version of the sACN library does not support sACN Sync. This paragraph will be valid in the future.
     *
     * @param[in] receiver_handle The merge receiver's handle.
     * @param[in] source_addr The network address from which the sACN packet originated.
     * @param[in] source_info Information about the source that sent this data.
     * @param[in] universe_data The universe data (and relevant information about that data), starting from the first
     * slot of the currently configured footprint.
     */
    virtual void HandleNonDmxData(Handle receiver_handle, const etcpal::SockAddr& source_addr,
                                  const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data)
    {
      ETCPAL_UNUSED_ARG(receiver_handle);
      ETCPAL_UNUSED_ARG(source_addr);
      ETCPAL_UNUSED_ARG(source_info);
      ETCPAL_UNUSED_ARG(universe_data);
    }

    /**
     * @brief Notify that one or more sources have entered a source loss state.
     * @param handle The merge receiver's handle.
     * @param universe The universe this merge receiver is monitoring.
     * @param lost_sources Vector of structs describing the source or sources that have been lost.
     */
    virtual void HandleSourcesLost(Handle handle, uint16_t universe, const std::vector<SacnLostSource>& lost_sources)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
      ETCPAL_UNUSED_ARG(lost_sources);
    }

    /**
     * @brief Notify that a merge receiver's sampling period has begun.
     *
     * If this sampling period was due to a networking reset, some sources may not be included in it. The sources that
     * are not part of the sampling period will continue to be included in merged data notifications.
     *
     * @param handle The merge receiver's handle.
     * @param universe The universe the merge receiver is monitoring.
     */
    virtual void HandleSamplingPeriodStarted(Handle handle, uint16_t universe)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
    }

    /**
     * @brief Notify that a merge receiver's sampling period has ended.
     *
     * All sources that were included in this sampling period will now officially be included in merged data
     * notifications. If there was a networking reset during this sampling period, another sampling period may have been
     * scheduled, in which case this will be immediately followed by a sampling period started notification.
     *
     * If there were any active levels received during the sampling period, they were factored into the merged data
     * notification called immediately before this notification. If the merged data notification wasn't called before
     * this notification, that means there currently isn't any active data on the universe.
     *
     * @param handle The merge receiver's handle.
     * @param universe The universe the merge receiver is monitoring.
     */
    virtual void HandleSamplingPeriodEnded(Handle handle, uint16_t universe)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
    }

    /**
     * @brief Notify that a source has stopped transmission of per-address priority packets.
     * @param handle The merge receiver's handle.
     * @param universe The universe this merge receiver is monitoring.
     * @param source Information about the source that has stopped transmission of per-address priority.
     */
    virtual void HandleSourcePapLost(Handle handle, uint16_t universe, const SacnRemoteSource& source)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
      ETCPAL_UNUSED_ARG(source);
    }

    /**
     * @brief Notify that more than the configured maximum number of sources are currently sending on
     *        the universe being listened to.
     *
     * This is a notification that is directly forwarded from the sACN Receiver module.
     *
     * @param[in] handle The merge receiver's handle.
     * @param[in] universe The universe this merge receiver is monitoring.
     */
    virtual void HandleSourceLimitExceeded(Handle handle, uint16_t universe)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
    }
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

    /** The footprint within the universe to monitor. TODO: Currently unimplemented and thus ignored. */
    SacnRecvUniverseSubrange footprint{1, DMX_ADDRESS_COUNT};

    /** The maximum number of sources this universe will listen to when using dynamic memory. */
    int source_count_max{SACN_RECEIVER_INFINITE_SOURCES};

    /** If true, this allows per-address priorities (if any are received) to be fed into the merger. If false, received
        per-address priorities are ignored, and only universe priorities are used in the merger. Keep in mind that this
        setting will be ignored if #SACN_ETC_PRIORITY_EXTENSION = 0, in which case per-address priorities are ignored.
     */
    bool use_pap{true};

    /** What IP networking the merge receiver will support. */
    sacn_ip_support_t ip_supported{kSacnIpV4AndIpV6};

    /** Create an empty, invalid data structure by default. */
    Settings() = default;

    /** Instantiates merge receiver settings based on a universe ID. This constructor is not marked explicit on purpose
        so that a Settings instance can be implicitly constructed from a universe number.
     */
    Settings(uint16_t new_universe_id);

    bool IsValid() const;
  };

  /**
   * @ingroup sacn_merge_receiver_cpp
   * @brief A set of network interfaces for a particular merge receiver.
   */
  struct NetintList
  {
    /** The merge receiver's handle. */
    sacn_merge_receiver_t handle{SACN_MERGE_RECEIVER_INVALID};

    /** If !empty, this is the list of interfaces the application wants to use, and the status codes are filled in. If
        empty, all available interfaces are tried. */
    std::vector<SacnMcastInterface> netints;

    /** If this is true, this merge receiver will not use any network interfaces for multicast traffic. */
    bool no_netints{false};

    /** Create an empty, invalid data structure by default. */
    NetintList() = default;
    NetintList(sacn_merge_receiver_t merge_receiver_handle, McastMode mcast_mode);
    NetintList(sacn_merge_receiver_t merge_receiver_handle, const std::vector<SacnMcastInterface>& network_interfaces);
  };

  /**
   * @ingroup sacn_merge_receiver_cpp
   * @brief Information about a remote sACN source being tracked by a merge receiver.
   */
  struct Source
  {
    /** The handle of the source. */
    sacn_remote_source_t handle;
    /** The Component Identifier (CID) of the source. */
    etcpal::Uuid cid;
    /** The name of the source. */
    std::string name;
    /** The network address from which the most recent sACN packet originated. */
    etcpal::SockAddr addr;
  };

  MergeReceiver() = default;
  MergeReceiver(const MergeReceiver& other) = delete;
  MergeReceiver& operator=(const MergeReceiver& other) = delete;
  MergeReceiver(MergeReceiver&& other) = default;            /**< Move a merge receiver instance. */
  MergeReceiver& operator=(MergeReceiver&& other) = default; /**< Move a merge receiver instance. */

  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler, McastMode mcast_mode);
  etcpal::Error Startup(const Settings& settings, NotifyHandler& notify_handler,
                        std::vector<SacnMcastInterface>& netints);
  void Shutdown();
  etcpal::Expected<uint16_t> GetUniverse() const;
  etcpal::Expected<SacnRecvUniverseSubrange> GetFootprint() const;
  etcpal::Error ChangeUniverse(uint16_t new_universe_id);
  etcpal::Error ChangeFootprint(const SacnRecvUniverseSubrange& new_footprint);
  etcpal::Error ChangeUniverseAndFootprint(uint16_t new_universe_id, const SacnRecvUniverseSubrange& new_footprint);
  std::vector<EtcPalMcastNetintId> GetNetworkInterfaces();
  etcpal::Expected<Source> GetSource(sacn_remote_source_t source_handle);

  static etcpal::Error ResetNetworking(McastMode mcast_mode);
  static etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints);
  static etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& sys_netints,
                                       std::vector<NetintList>& netint_lists);

  constexpr Handle handle() const;

private:
  SacnMergeReceiverConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_;
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void MergeReceiverCbMergedData(sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data,
                                                 void* context)
{
  if (context && merged_data)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleMergedData(MergeReceiver::Handle(handle), *merged_data);
  }
}

extern "C" inline void MergeReceiverCbNonDmx(sacn_merge_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                             const SacnRemoteSource* source_info,
                                             const SacnRecvUniverseData* universe_data, void* context)
{
  if (context && source_addr && source_info && universe_data)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleNonDmxData(MergeReceiver::Handle(receiver_handle),
                                                                          *source_addr, *source_info, *universe_data);
  }
}

extern "C" inline void MergeReceiverCbSourcesLost(sacn_merge_receiver_t handle, uint16_t universe,
                                                  const SacnLostSource* lost_sources, size_t num_lost_sources,
                                                  void* context)
{
  if (context && lost_sources && (num_lost_sources > 0))
  {
    std::vector<SacnLostSource> lost_vec(lost_sources, lost_sources + num_lost_sources);
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSourcesLost(MergeReceiver::Handle(handle), universe,
                                                                           lost_vec);
  }
}

extern "C" inline void MergeReceiverCbSamplingPeriodStarted(sacn_merge_receiver_t handle, uint16_t universe,
                                                            void* context)
{
  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSamplingPeriodStarted(MergeReceiver::Handle(handle),
                                                                                     universe);
  }
}

extern "C" inline void MergeReceiverCbSamplingPeriodEnded(sacn_merge_receiver_t handle, uint16_t universe,
                                                          void* context)
{
  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSamplingPeriodEnded(MergeReceiver::Handle(handle),
                                                                                   universe);
  }
}

extern "C" inline void MergeReceiverCbSourcePapLost(sacn_merge_receiver_t handle, uint16_t universe,
                                                    const SacnRemoteSource* source, void* context)
{
  if (context && source)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSourcePapLost(MergeReceiver::Handle(handle),
                                                                             universe, *source);
  }
}

extern "C" inline void MergeReceiverCbSourceLimitExceeded(sacn_merge_receiver_t handle, uint16_t universe,
                                                          void* context)
{
  if (context)
  {
    static_cast<MergeReceiver::NotifyHandler*>(context)->HandleSourceLimitExceeded(MergeReceiver::Handle(handle),
                                                                                   universe);
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
  return (universe_id > 0) && (footprint.start_address >= 1) && (footprint.start_address <= DMX_ADDRESS_COUNT) &&
         (footprint.address_count >= 1) &&
         (footprint.address_count <= (DMX_ADDRESS_COUNT - footprint.start_address + 1));
}

/**
 * @brief Create a Netint List instance by passing the required members explicitly.
 *
 * Optional members can be modified directly in the struct.
 */
inline MergeReceiver::NetintList::NetintList(sacn_merge_receiver_t merge_receiver_handle,
                                             McastMode mcast_mode = McastMode::kEnabledOnAllInterfaces)
    : handle(merge_receiver_handle), no_netints(mcast_mode == McastMode::kDisabledOnAllInterfaces)
{
}

/**
 * @brief Create a Netint List instance by passing the required members explicitly.
 *
 * This constructor enables the use of list initialization when setting up one or more NetintLists (such as
 * initializing the vector<NetintList> that gets passed into MergeReceiver::ResetNetworking).
 */
inline MergeReceiver::NetintList::NetintList(sacn_merge_receiver_t merge_receiver_handle,
                                             const std::vector<SacnMcastInterface>& network_interfaces)
    : handle(merge_receiver_handle), netints(network_interfaces)
{
}

/**
 * @brief Start listening for sACN data on a universe.
 *
 * This is an overload of Startup that defaults to using all system interfaces for multicast traffic, but can also be
 * used to disable multicast traffic on all interfaces.
 *
 * An sACN merge receiver can listen on one universe at a time, and each universe can only be listened to
 * by one merge receiver at at time.
 *
 * Note that a merge receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN merge receiver and this class instance.
 * @param[in] notify_handler The notification interface to call back to the application.
 * @param[in] mcast_mode This controls whether or not multicast traffic is allowed for this merge receiver.
 * @return #kEtcPalErrOk: Merge Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A merge receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this merge receiver, or maximum merge receivers reached.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                            McastMode mcast_mode = McastMode::kEnabledOnAllInterfaces)
{
  SacnMergeReceiverConfig config = TranslateConfig(settings, notify_handler);

  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  if (mcast_mode == McastMode::kDisabledOnAllInterfaces)
    netint_config.no_netints = true;

  sacn_merge_receiver_t c_handle = SACN_MERGE_RECEIVER_INVALID;
  etcpal::Error result = sacn_merge_receiver_create(&config, &c_handle, &netint_config);

  handle_.SetValue(c_handle);

  return result;
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
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
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

  sacn_merge_receiver_t c_handle = SACN_MERGE_RECEIVER_INVALID;
  etcpal::Error result = kEtcPalErrOk;

  if (netints.empty())
  {
    result = sacn_merge_receiver_create(&config, &c_handle, NULL);
  }
  else
  {
    SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
    netint_config.netints = netints.data();
    netint_config.num_netints = netints.size();

    result = sacn_merge_receiver_create(&config, &c_handle, &netint_config);
  }

  handle_.SetValue(c_handle);

  return result;
}

/**
 * @brief Stop listening for sACN data on a universe.
 *
 * Tears down the merge receiver and any sources currently being tracked on the merge receiver's universe.
 * Stops listening for sACN on that universe.
 */
inline void MergeReceiver::Shutdown()
{
  sacn_merge_receiver_destroy(handle_.value());
  handle_.Clear();
}

/**
 * @brief Get the universe this merge receiver is listening to.
 *
 * @return If valid, the value is the universe id.  Otherwise, this is the underlying error the C library call returned.
 */
inline etcpal::Expected<uint16_t> MergeReceiver::GetUniverse() const
{
  uint16_t result = 0;
  etcpal_error_t err = sacn_merge_receiver_get_universe(handle_.value(), &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/**
 * @brief Get the footprint within the universe this merge receiver is listening to.
 *
 * TODO: At this time, custom footprints are not supported by this library, so the full 512-slot footprint is returned.
 *
 * @return If valid, the value is the footprint.  Otherwise, this is the underlying error the C library call returned.
 */
inline etcpal::Expected<SacnRecvUniverseSubrange> MergeReceiver::GetFootprint() const
{
  SacnRecvUniverseSubrange result;
  etcpal_error_t err = sacn_merge_receiver_get_footprint(handle_.value(), &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/**
 * @brief Change the universe this class is listening to.
 *
 * An sACN merge receiver can only listen on one universe at a time. After this call completes, a new sampling period
 * will occur, and then underlying updates will generate new calls to HandleMergedData(). If this call fails, the caller
 * must call Shutdown() on this class, because it may be in an invalid state.
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
  return sacn_merge_receiver_change_universe(handle_.value(), new_universe_id);
}

/**
 * @brief Change the footprint within the universe this merge receiver is listening to. TODO: Not yet implemented.
 *
 * After this call completes, a new sampling period will occur, and then underlying updates will generate new calls to
 * HandleMergedData().
 *
 * @param[in] new_footprint New footprint that this merge receiver should listen to.
 * @return #kEtcPalErrNotImpl: Not yet implemented.
 */
inline etcpal::Error MergeReceiver::ChangeFootprint(const SacnRecvUniverseSubrange& new_footprint)
{
  return sacn_merge_receiver_change_footprint(handle_.value(), &new_footprint);
}

/**
 * @brief Change the universe and footprint this merge receiver is listening to. TODO: Not yet implemented.
 *
 * After this call completes, a new sampling period will occur, and then underlying updates will generate new calls to
 * HandleMergedData().
 *
 * @param[in] new_universe_id New universe number that this merge receiver should listen to.
 * @param[in] new_footprint New footprint within the universe.
 * @return #kEtcPalErrNotImpl: Not yet implemented.
 */
inline etcpal::Error MergeReceiver::ChangeUniverseAndFootprint(uint16_t new_universe_id,
                                                               const SacnRecvUniverseSubrange& new_footprint)
{
  return sacn_merge_receiver_change_universe_and_footprint(handle_.value(), new_universe_id, &new_footprint);
}

/**
 * @brief Obtain a vector of this merge receiver's network interfaces.
 *
 * @return A vector of this merge receiver's network interfaces.
 */
inline std::vector<EtcPalMcastNetintId> MergeReceiver::GetNetworkInterfaces()
{
  // This uses a guessing algorithm with a while loop to avoid race conditions.
  std::vector<EtcPalMcastNetintId> netints;
  size_t size_guess = 4u;
  size_t num_netints = 0u;

  do
  {
    netints.resize(size_guess);
    num_netints = sacn_merge_receiver_get_network_interfaces(handle_.value(), netints.data(), netints.size());
    size_guess = num_netints + 4u;
  } while (num_netints > netints.size());

  netints.resize(num_netints);
  return netints;
}

/**
 * @brief Gets a copy of the information for the specified merge receiver source.
 *
 * @param[in] source_handle Handle to the source to obtain information for.
 * @return A copy of the source's information if found.
 * @return #kEtcPalErrNotFound: The merge receiver has no knowledge of the specified source.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Expected<MergeReceiver::Source> MergeReceiver::GetSource(sacn_remote_source_t source_handle)
{
  SacnMergeReceiverSource c_info;
  etcpal_error_t error = sacn_merge_receiver_get_source(handle_.value(), source_handle, &c_info);
  if (error == kEtcPalErrOk)
  {
    MergeReceiver::Source res;
    res.handle = c_info.handle;
    res.cid = c_info.cid;
    res.name = c_info.name;
    res.addr = c_info.addr;
    return res;
  }

  return error;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for all sACN merge receivers.
 *
 * This is an overload of ResetNetworking that defaults to using all system interfaces for multicast traffic, but can
 * also be used to disable multicast traffic on all interfaces.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. The receiver
 * (and by extension, merge receiver) API will no longer be limited to specific interfaces (the list passed into
 * sacn::Init(), if any, is overridden for the receiver API, but not the other APIs). Every receiver (including every
 * merge receiver) is set to all system interfaces.
 *
 * After this call completes, a new sampling period occurs, and then underlying updates will generate new calls to
 * HandleMergedData(). If this call fails, the caller must call Shutdown() for each merge receiver, because the merge
 * receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces. This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] mcast_mode This controls whether or not multicast traffic is allowed for this merge receiver.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::ResetNetworking(McastMode mcast_mode = McastMode::kEnabledOnAllInterfaces)
{
  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  if (mcast_mode == McastMode::kDisabledOnAllInterfaces)
    netint_config.no_netints = true;

  return sacn_merge_receiver_reset_networking(&netint_config);
}

/**
 * @brief Resets underlying network sockets and packet receipt state, determines network interfaces for all merge
 * receivers.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the receiver (and by extension, merge receiver) API will be limited to (the list passed
 * into sacn::Init(), if any, is overridden for the receiver API, but not the other APIs). Then all receivers (including
 * merge receivers) will be configured to use all of those interfaces.
 *
 * After this call completes, a new sampling period occurs, and then underlying updates will generate new calls to
 * HandleMergedData(). If this call fails, the caller must call Shutdown() for each merge receiver, because the merge
 * receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in. This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param sys_netints If !empty, this is the list of system interfaces the receiver API will be limited to, and the
 * status codes are filled in.  If empty, the receiver API is allowed to use all available system interfaces.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::ResetNetworking(std::vector<SacnMcastInterface>& sys_netints)
{
  if (sys_netints.empty())
    return sacn_merge_receiver_reset_networking(nullptr);

  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.netints = sys_netints.data();
  netint_config.num_netints = sys_netints.size();

  return sacn_merge_receiver_reset_networking(&netint_config);
}

/**
 * @brief Resets underlying network sockets and packet receipt state, determines network interfaces for each merge
 * receiver.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the receiver (and by extension, merge receiver) API will be limited to (the list passed
 * into sacn::Init(), if any, is overridden for the receiver API, but not the other APIs). Then the network interfaces
 * are specified for each merge receiver.
 *
 * After this call completes, a new sampling period occurs, and then underlying updates will generate new calls to
 * HandleMergedData(). If this call fails, the caller must call Shutdown() for each merge receiver, because the merge
 * receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the network
 * interfaces passed in for each merge receiver. This will only return #kEtcPalErrNoNetints if none of the interfaces
 * work for a merge receiver.
 *
 * @param sys_netints If !empty, this is the list of system interfaces the receiver API will be limited to, and the
 * status codes are filled in.  If empty, the receiver API is allowed to use all available system interfaces.
 * @param[in, out] per_receiver_netint_lists Vector of lists of interfaces the application wants to use for each merge
 * receiver. Must not be empty. Must include all merge receivers, and nothing more. The status codes are filled in
 * whenever MergeReceiver::NetintList::netints is !empty.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided for a merge receiver were usable by the
 * library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error MergeReceiver::ResetNetworking(std::vector<SacnMcastInterface>& sys_netints,
                                                    std::vector<NetintList>& per_receiver_netint_lists)
{
  std::vector<SacnMergeReceiverNetintList> netint_lists_c;
  netint_lists_c.reserve(per_receiver_netint_lists.size());
  std::transform(
      per_receiver_netint_lists.begin(), per_receiver_netint_lists.end(), std::back_inserter(netint_lists_c),
      [](NetintList& list) {
        // clang-format off
                   SacnMergeReceiverNetintList c_list = {
                     list.handle,
                     list.netints.data(),
                     list.netints.size(),
                     list.no_netints
                   };
        // clang-format on

        return c_list;
      });

  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.netints = sys_netints.data();
  netint_config.num_netints = sys_netints.size();

  return sacn_merge_receiver_reset_networking_per_receiver(&netint_config, netint_lists_c.data(),
                                                           netint_lists_c.size());
}

/**
 * @brief Get the current handle to the underlying C merge receiver.
 *
 * @return The handle, which will only be valid if the merge receiver has been successfully created using Startup().
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
      internal::MergeReceiverCbSourcesLost,
      internal::MergeReceiverCbSamplingPeriodStarted,
      internal::MergeReceiverCbSamplingPeriodEnded,
      internal::MergeReceiverCbSourcePapLost,
      internal::MergeReceiverCbSourceLimitExceeded,
      &notify_handler
    },
    settings.footprint,
    settings.source_count_max,
    settings.use_pap,
    settings.ip_supported
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_MERGE_RECEIVER_H_
