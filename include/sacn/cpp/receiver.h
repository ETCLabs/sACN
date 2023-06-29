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

#ifndef SACN_CPP_RECEIVER_H_
#define SACN_CPP_RECEIVER_H_

/**
 * @file sacn/cpp/receiver.h
 * @brief C++ wrapper for the sACN Receiver API
 */

#include <vector>

#include "sacn/cpp/common.h"

#include "sacn/receiver.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/opaque_id.h"

/**
 * @defgroup sacn_receiver_cpp sACN Receiver API
 * @ingroup sacn_cpp_api
 * @brief A C++ wrapper for the sACN Receiver API
 */

namespace sacn
{

namespace detail
{
class ReceiverHandleType
{
};
};  // namespace detail

/**
 * @ingroup sacn_receiver_cpp
 * @brief An instance of sACN Receiver functionality; see @ref using_receiver.
 *
 * Components that receive sACN are referred to as sACN Receivers. Use this API to act as an sACN
 * Receiver.
 *
 * See @ref using_receiver for a detailed description of how to use this API.
 */
class Receiver
{
public:
  /** A handle type used by the sACN library to identify receiver instances. */
  using Handle = etcpal::OpaqueId<detail::ReceiverHandleType, sacn_receiver_t, SACN_RECEIVER_INVALID>;

  /**
   * @ingroup sacn_receiver_cpp
   * @brief A base class for a class that receives notification callbacks from a sACN receiver.
   */
  class NotifyHandler
  {
  public:
    virtual ~NotifyHandler() = default;

    /**
     * @brief Notify that new universe data within the configured footprint has been received.
     *
     * This will not be called if the Stream_Terminated bit is set, or if the Preview_Data bit is set and preview
     * packets are being filtered.
     *
     * During the sampling period, any valid sACN data packet received will trigger this notification, no matter the
     * start code.
     *
     * After the sampling period, if #SACN_ETC_PRIORITY_EXTENSION is set to 1, NULL start code packets will not trigger
     * this notification until either the PAP timeout expires or a PAP (0xDD) packet is received. PAP packets received
     * will always trigger this notification. This guarantees that if both start codes are active, PAP will always
     * notify first. All other start codes will always trigger this notification once received. If
     * #SACN_ETC_PRIORITY_EXTENSION is set to 0, NULL start code packets received will always trigger this notification.
     *
     * If the source is sending sACN Sync packets, this callback will only be called when the sync packet is received,
     * if the source forces the packet, or if the source sends a data packet without a sync universe.
     * TODO: this version of the sACN library does not support sACN Sync. This paragraph will be valid in the future.
     *
     * @param receiver_handle The receiver's handle.
     * @param source_addr IP address & port of the packet source.
     * @param source_info Information about the source that sent this data.
     * @param universe_data The universe data (and relevant information about that data), starting from the first slot
     * of the currently configured footprint.
     */
    virtual void HandleUniverseData(Handle receiver_handle, const etcpal::SockAddr& source_addr,
                                    const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data) = 0;

    /**
     * @brief Notify that one or more sources have entered a source loss state.
     * @param handle The receiver's handle.
     * @param universe The universe this receiver is monitoring.
     * @param lost_sources Vector of structs describing the source or sources that have been lost.
     */
    virtual void HandleSourcesLost(Handle handle, uint16_t universe,
                                   const std::vector<SacnLostSource>& lost_sources) = 0;

    /**
     * @brief Notify that a receiver's sampling period has begun.
     *
     * If this sampling period was due to a networking reset, some sources may not be included in it. See the universe
     * data callback to determine if a source is included or not.
     *
     * @param handle The receiver's handle.
     * @param universe The universe the receiver is monitoring.
     */
    virtual void HandleSamplingPeriodStarted(Handle handle, uint16_t universe)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
    }

    /**
     * @brief Notify that a receiver's sampling period has ended.
     *
     * All sources that were included in this sampling period can officially be used in the merge result for the
     * universe. If there was a networking reset during this sampling period, another sampling period may have been
     * scheduled, in which case this will be immediately followed by a sampling period started notification.
     *
     * @param handle The receiver's handle.
     * @param universe The universe the receiver is monitoring.
     */
    virtual void HandleSamplingPeriodEnded(Handle handle, uint16_t universe)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
    }

    /**
     * @brief Notify that a source has stopped transmission of per-address priority packets.
     * @param handle The receiver's handle.
     * @param universe The universe this receiver is monitoring.
     * @param source Information about the source that has stopped transmission of per-address priority.
     */
    virtual void HandleSourcePapLost(Handle handle, uint16_t universe, const SacnRemoteSource& source)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
      ETCPAL_UNUSED_ARG(source);
    }

    /**
     * @brief Notify that more than the configured maximum number of sources are currently sending on the universe
     * being listened to.
     * @param handle The receiver's handle.
     * @param universe The universe this receiver is monitoring.
     */
    virtual void HandleSourceLimitExceeded(Handle handle, uint16_t universe)
    {
      ETCPAL_UNUSED_ARG(handle);
      ETCPAL_UNUSED_ARG(universe);
    }
  };

  /**
   * @ingroup sacn_receiver_cpp
   * @brief A set of configuration settings that a receiver needs to initialize.
   */
  struct Settings
  {
    /********* Required values **********/

    uint16_t universe_id{0}; /**< The sACN universe number the receiver is listening to. */

    /********* Optional values **********/

    /** The footprint within the universe to monitor. TODO: Currently unimplemented and thus ignored. */
    SacnRecvUniverseSubrange footprint{1, DMX_ADDRESS_COUNT};

    /** The maximum number of sources this universe will listen to.  May be #SACN_RECEIVER_INFINITE_SOURCES.
        When configured to use static memory, this parameter is only used if it's less than
        #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE -- otherwise #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE is used instead.*/
    int source_count_max{SACN_RECEIVER_INFINITE_SOURCES};
    unsigned int flags{0}; /**< A set of option flags. See the C API's "sACN receiver flags". */

    sacn_ip_support_t ip_supported{kSacnIpV4AndIpV6}; /**< What IP networking the receiver will support. */

    /** Create an empty, invalid data structure by default. */
    Settings() = default;

    /** Instantiates receiver settings based on a universe ID. This constructor is not marked explicit on purpose so
        that a Settings instance can be implicitly constructed from a universe number.
     */
    Settings(uint16_t new_universe_id);

    bool IsValid() const;
  };

  /**
   * @ingroup sacn_receiver_cpp
   * @brief A set of network interfaces for a particular receiver.
   */
  struct NetintList
  {
    /** The receiver's handle. */
    sacn_receiver_t handle{SACN_RECEIVER_INVALID};

    /** If !empty, this is the list of interfaces the application wants to use, and the status codes are filled in. If
        empty, all available interfaces are tried. */
    std::vector<SacnMcastInterface> netints;

    /** If this is true, this receiver will not use any network interfaces for multicast traffic. */
    bool no_netints{false};

    /** Create an empty, invalid data structure by default. */
    NetintList() = default;
    NetintList(sacn_receiver_t receiver_handle, McastMode mcast_mode);
    NetintList(sacn_receiver_t receiver_handle, const std::vector<SacnMcastInterface>& network_interfaces);
  };

  Receiver() = default;
  Receiver(const Receiver& other) = delete;
  Receiver& operator=(const Receiver& other) = delete;
  Receiver(Receiver&& other) = default;            /**< Move a device instance. */
  Receiver& operator=(Receiver&& other) = default; /**< Move a device instance. */

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

  // Lesser used functions.  These apply to all instances of this class.
  static void SetExpiredWait(uint32_t wait_ms);
  static uint32_t GetExpiredWait();

  static etcpal::Error ResetNetworking(McastMode mcast_mode);
  static etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints);
  static etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& sys_netints,
                                       std::vector<NetintList>& netint_lists);

  constexpr Handle handle() const;

private:
  SacnReceiverConfig TranslateConfig(const Settings& settings, NotifyHandler& notify_handler);

  Handle handle_;
};

/**
 * @cond device_c_callbacks
 * Callbacks from underlying device library to be forwarded
 */
namespace internal
{
extern "C" inline void ReceiverCbUniverseData(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                              const SacnRemoteSource* source_info,
                                              const SacnRecvUniverseData* universe_data, void* context)
{
  if (source_addr && source_info && universe_data && context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleUniverseData(Receiver::Handle(receiver_handle), *source_addr,
                                                                       *source_info, *universe_data);
  }
}

extern "C" inline void ReceiverCbSourcesLost(sacn_receiver_t handle, uint16_t universe,
                                             const SacnLostSource* lost_sources, size_t num_lost_sources, void* context)
{
  if (context && lost_sources && (num_lost_sources > 0))
  {
    std::vector<SacnLostSource> lost_vec(lost_sources, lost_sources + num_lost_sources);
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourcesLost(Receiver::Handle(handle), universe, lost_vec);
  }
}

extern "C" inline void ReceiverCbSamplingPeriodStarted(sacn_receiver_t handle, uint16_t universe, void* context)
{
  if (context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSamplingPeriodStarted(Receiver::Handle(handle), universe);
  }
}

extern "C" inline void ReceiverCbSamplingPeriodEnded(sacn_receiver_t handle, uint16_t universe, void* context)
{
  if (context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSamplingPeriodEnded(Receiver::Handle(handle), universe);
  }
}

extern "C" inline void ReceiverCbPapLost(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source,
                                         void* context)
{
  if (context && source)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourcePapLost(Receiver::Handle(handle), universe, *source);
  }
}

extern "C" inline void ReceiverCbSourceLimitExceeded(sacn_receiver_t handle, uint16_t universe, void* context)
{
  if (context)
  {
    static_cast<Receiver::NotifyHandler*>(context)->HandleSourceLimitExceeded(Receiver::Handle(handle), universe);
  }
}

};  // namespace internal

/**
 * @endcond
 */

/**
 * @brief Create a Receiver Settings instance by passing the required members explicitly.
 *
 * Optional members can be modified directly in the struct.
 */
inline Receiver::Settings::Settings(uint16_t new_universe_id) : universe_id(new_universe_id)
{
}

/**
 * Determine whether a Reciever Settings instance contains valid data for sACN operation.
 */
inline bool Receiver::Settings::IsValid() const
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
inline Receiver::NetintList::NetintList(sacn_receiver_t receiver_handle,
                                        McastMode mcast_mode = McastMode::kEnabledOnAllInterfaces)
    : handle(receiver_handle), no_netints(mcast_mode == McastMode::kDisabledOnAllInterfaces)
{
}

/**
 * @brief Create a Netint List instance by passing the required members explicitly.
 *
 * This constructor enables the use of list initialization when setting up one or more NetintLists (such as
 * initializing the vector<NetintList> that gets passed into MergeReceiver::ResetNetworking).
 */
inline Receiver::NetintList::NetintList(sacn_receiver_t receiver_handle,
                                        const std::vector<SacnMcastInterface>& network_interfaces)
    : handle(receiver_handle), netints(network_interfaces)
{
}

/**
 * @brief Start listening for sACN data on a universe.
 *
 * This is an overload of Startup that defaults to using all system interfaces for multicast traffic, but can also be
 * used to disable multicast traffic on all interfaces.
 *
 * An sACN receiver can listen on one universe at a time, and each universe can only be listened to
 * by one receiver at at time.
 *
 * Note that a receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN receiver and this class instance.
 * @param[in] notify_handler The notification interface to call back to the application.
 * @param[in] mcast_mode This controls whether or not multicast traffic is allowed for this receiver.
 * @return #kEtcPalErrOk: Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                       McastMode mcast_mode = McastMode::kEnabledOnAllInterfaces)
{
  SacnReceiverConfig config = TranslateConfig(settings, notify_handler);

  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  if (mcast_mode == McastMode::kDisabledOnAllInterfaces)
    netint_config.no_netints = true;

  sacn_receiver_t c_handle = SACN_RECEIVER_INVALID;
  etcpal::Error result = sacn_receiver_create(&config, &c_handle, &netint_config);

  handle_.SetValue(c_handle);

  return result;
}

/**
 * @brief Start listening for sACN data on a universe.
 *
 * An sACN receiver can listen on one universe at a time, and each universe can only be listened to by one receiver at
 * at time.
 *
 * After this call completes successfully, the receiver is in a sampling period for the universe and will provide
 * HandleSamplingPeriodStarted() and HandleSamplingPeriodEnded() notifications, as well as HandleUniverseData()
 * notifications as packets are received for the universe.
 *
 * Note that a receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN receiver and this class instance.
 * @param[in] notify_handler The notification interface to call back to the application.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::Startup(const Settings& settings, NotifyHandler& notify_handler,
                                       std::vector<SacnMcastInterface>& netints)
{
  SacnReceiverConfig config = TranslateConfig(settings, notify_handler);

  sacn_receiver_t c_handle = SACN_RECEIVER_INVALID;
  etcpal::Error result = kEtcPalErrOk;

  if (netints.empty())
  {
    result = sacn_receiver_create(&config, &c_handle, NULL);
  }
  else
  {
    SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
    netint_config.netints = netints.data();
    netint_config.num_netints = netints.size();

    result = sacn_receiver_create(&config, &c_handle, &netint_config);
  }

  handle_.SetValue(c_handle);

  return result;
}

/**
 * @brief Stop listening for sACN data on a universe.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 */
inline void Receiver::Shutdown()
{
  sacn_receiver_destroy(handle_.value());
  handle_.Clear();
}

/**
 * @brief Get the universe this receiver is listening to.
 *
 * @return If valid, the value is the universe id.  Otherwise, this is the underlying error the C library call returned.
 */
inline etcpal::Expected<uint16_t> Receiver::GetUniverse() const
{
  uint16_t result = 0;
  etcpal_error_t err = sacn_receiver_get_universe(handle_.value(), &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/**
 * @brief Get the footprint within the universe this receiver is listening to.
 *
 * TODO: At this time, custom footprints are not supported by this library, so the full 512-slot footprint is returned.
 *
 * @return If valid, the value is the footprint.  Otherwise, this is the underlying error the C library call returned.
 */
inline etcpal::Expected<SacnRecvUniverseSubrange> Receiver::GetFootprint() const
{
  SacnRecvUniverseSubrange result;
  etcpal_error_t err = sacn_receiver_get_footprint(handle_.value(), &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/**
 * @brief Change the universe this receiver is listening to.
 *
 * An sACN receiver can only listen on one universe at a time. After this call completes successfully, the receiver is
 * in a sampling period for the new universe and will provide HandleSamplingPeriodStarted() and
 * HandleSamplingPeriodEnded() notifications, as well as HandleUniverseData() notifications as packets are received for
 * the new universe. If this call fails, the caller must call Shutdown() on this class, because it may be in an invalid
 * state.
 *
 * @param[in] new_universe_id New universe number that this receiver should listen to.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified new universe.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::ChangeUniverse(uint16_t new_universe_id)
{
  return sacn_receiver_change_universe(handle_.value(), new_universe_id);
}

/**
 * @brief Change the footprint within the universe this receiver is listening to. TODO: Not yet implemented.
 *
 * After this call completes successfully, the receiver is in a sampling period for the new footprint and will provide
 * HandleSamplingPeriodStarted() and HandleSamplingPeriodEnded() notifications, as well as HandleUniverseData()
 * notifications as packets are received for the new footprint.
 *
 * @param[in] new_footprint New footprint that this receiver should listen to.
 * @return #kEtcPalErrNotImpl: Not yet implemented.
 */
inline etcpal::Error Receiver::ChangeFootprint(const SacnRecvUniverseSubrange& new_footprint)
{
  return sacn_receiver_change_footprint(handle_.value(), &new_footprint);
}

/**
 * @brief Change the universe and footprint this receiver is listening to. TODO: Not yet implemented.
 *
 * After this call completes successfully, the receiver is in a sampling period for the new footprint and will provide
 * HandleSamplingPeriodStarted() and HandleSamplingPeriodEnded() notifications, as well as HandleUniverseData()
 * notifications as packets are received for the new footprint.
 *
 * @param[in] new_universe_id New universe number that this receiver should listen to.
 * @param[in] new_footprint New footprint within the universe.
 * @return #kEtcPalErrNotImpl: Not yet implemented.
 */
inline etcpal::Error Receiver::ChangeUniverseAndFootprint(uint16_t new_universe_id,
                                                          const SacnRecvUniverseSubrange& new_footprint)
{
  return sacn_receiver_change_universe_and_footprint(handle_.value(), new_universe_id, &new_footprint);
}

/**
 * @brief Obtain a vector of this receiver's network interfaces.
 *
 * @return A vector of this receiver's network interfaces.
 */
inline std::vector<EtcPalMcastNetintId> Receiver::GetNetworkInterfaces()
{
  // This uses a guessing algorithm with a while loop to avoid race conditions.
  std::vector<EtcPalMcastNetintId> netints;
  size_t size_guess = 4u;
  size_t num_netints = 0u;

  do
  {
    netints.resize(size_guess);
    num_netints = sacn_receiver_get_network_interfaces(handle_.value(), netints.data(), netints.size());
    size_guess = num_netints + 4u;
  } while (num_netints > netints.size());

  netints.resize(num_netints);
  return netints;
}

/**
 * @brief Set the expired notification wait time.
 *
 * The library will wait at least this long after a source loss condition has been encountered before
 * calling HandleSourcesLost(). However, the wait may be longer due to the source loss algorithm (see \ref
 * source_loss_behavior).
 *
 * @param[in] wait_ms Wait time in milliseconds.
 */
inline void Receiver::SetExpiredWait(uint32_t wait_ms)
{
  sacn_receiver_set_expired_wait(wait_ms);
}

/**
 * @brief Get the current value of the expired notification wait time.
 *
 * The library will wait at least this long after a source loss condition has been encountered before
 * calling HandleSourcesLost(). However, the wait may be longer due to the source loss algorithm (see \ref
 * source_loss_behavior).
 *
 * @return Wait time in milliseconds.
 */
inline uint32_t Receiver::GetExpiredWait()
{
  return sacn_receiver_get_expired_wait();
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for all sACN receivers.
 *
 * This is an overload of ResetNetworking that defaults to using all system interfaces for multicast traffic, but can
 * also be used to disable multicast traffic on all interfaces.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. The receiver
 * API will no longer be limited to specific interfaces (the list passed into sacn::Init(), if any, is overridden for
 * the receiver API, but not the other APIs). Every receiver is set to all system interfaces.
 *
 * After this call completes successfully, every receiver is in a sampling period for their universes and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for their universes. If this call fails, the caller must call Shutdown() for each receiver,
 * because the receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces. This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] mcast_mode This controls whether or not multicast traffic is allowed for this receiver.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::ResetNetworking(McastMode mcast_mode = McastMode::kEnabledOnAllInterfaces)
{
  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  if (mcast_mode == McastMode::kDisabledOnAllInterfaces)
    netint_config.no_netints = true;

  return sacn_receiver_reset_networking(&netint_config);
}

/**
 * @brief Resets underlying network sockets and packet receipt state, determines network interfaces for all receivers.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the receiver API will be limited to (the list passed into sacn::Init(), if any, is
 * overridden for the receiver API, but not the other APIs). Then all receivers will be configured to use all of those
 * interfaces.
 *
 * After this call completes successfully, every receiver is in a sampling period for their universes and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for their universes. If this call fails, the caller must call Shutdown() for each receiver,
 * because the receivers may be in an invalid state.
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
inline etcpal::Error Receiver::ResetNetworking(std::vector<SacnMcastInterface>& sys_netints)
{
  if (sys_netints.empty())
    return sacn_receiver_reset_networking(nullptr);

  SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  netint_config.netints = sys_netints.data();
  netint_config.num_netints = sys_netints.size();

  return sacn_receiver_reset_networking(&netint_config);
}

/**
 * @brief Resets underlying network sockets and packet receipt state, determines network interfaces for each receiver.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed. This changes
 * the list of system interfaces the receiver API will be limited to (the list passed into sacn::Init(), if any, is
 * overridden for the receiver API, but not the other APIs). Then the network interfaces are specified for each
 * receiver.
 *
 * After this call completes successfully, every receiver is in a sampling period for their universes and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for their universes. If this call fails, the caller must call Shutdown() for each receiver,
 * because the receivers may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the network
 * interfaces passed in for each receiver. This will only return #kEtcPalErrNoNetints if none of the interfaces work for
 * a receiver.
 *
 * @param sys_netints If !empty, this is the list of system interfaces the receiver API will be limited to, and the
 * status codes are filled in.  If empty, the receiver API is allowed to use all available system interfaces.
 * @param[in, out] per_receiver_netint_lists Vector of lists of interfaces the application wants to use for each
 * receiver. Must not be empty. Must include all receivers, and nothing more. The status codes are filled in whenever
 * Receiver::NetintList::netints is !empty.
 * @return #kEtcPalErrOk: Networking reset successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided for a receiver were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Receiver::ResetNetworking(std::vector<SacnMcastInterface>& sys_netints,
                                               std::vector<NetintList>& per_receiver_netint_lists)
{
  std::vector<SacnReceiverNetintList> netint_lists_c;
  netint_lists_c.reserve(per_receiver_netint_lists.size());
  std::transform(
      per_receiver_netint_lists.begin(), per_receiver_netint_lists.end(), std::back_inserter(netint_lists_c),
      [](NetintList& list) {
        // clang-format off
                   SacnReceiverNetintList c_list = {
                     list.handle,
                     list.netints.data(),
                     list.netints.size(),
                     list.no_netints
                   };
        // clang-format on

        return c_list;
      });

  SacnNetintConfig sys_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  sys_netint_config.netints = sys_netints.data();
  sys_netint_config.num_netints = sys_netints.size();

  return sacn_receiver_reset_networking_per_receiver(&sys_netint_config, netint_lists_c.data(), netint_lists_c.size());
}

/**
 * @brief Get the current handle to the underlying C receiver.
 *
 * @return The handle, which will only be valid if the receiver has been successfully created using Startup().
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
      internal::ReceiverCbUniverseData,
      internal::ReceiverCbSourcesLost,
      internal::ReceiverCbSamplingPeriodStarted,
      internal::ReceiverCbSamplingPeriodEnded,
      internal::ReceiverCbPapLost,
      internal::ReceiverCbSourceLimitExceeded,
      &notify_handler
    },
    settings.footprint,
    settings.source_count_max,
    settings.flags,
    settings.ip_supported
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_RECEIVER_H_
