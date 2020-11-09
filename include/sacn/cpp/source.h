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

/**
 * @file sacn/cpp/source.h
 * @brief C++ wrapper for the sACN Source API
 */

#include <cstring>
#include "sacn/source.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal/cpp/inet.h"

/**
 * @defgroup sacn_source_cpp sACN Source API
 * @ingroup sacn_cpp_api
 * @brief A C++ wrapper for the sACN Source API
 */

namespace sacn
{
/**
 * @ingroup sacn_source_cpp
 * @brief An instance of sACN Source functionality; see @ref using_source.
 *
 * Components that send sACN are referred to as sACN Sources. Use this API to act as an sACN Source.
 * 
 * See @ref using_source for a detailed description of how to use this API.
 */
class Source
{
public:
  /** A handle type used by the sACN library to identify source instances. */
  using Handle = sacn_source_t;
  /** An invalid Handle value. */
  static constexpr Handle kInvalidHandle = SACN_SOURCE_INVALID;

  /**
   * @ingroup sacn_source_cpp
   * @brief A set of configuration settings that a source needs to initialize.
   */
  struct Settings
  {
    /********* Required values **********/

    /** The source's CID. */
    etcpal::Uuid cid;
    /** The source's name, a UTF-8 encoded string. Up to #SACN_SOURCE_NAME_MAX_LEN characters will be used. */
    std::string name;

    /********* Optional values **********/

    /** The maximum number of universes this source will send to when using dynamic memory. */
    size_t universe_count_max{SACN_SOURCE_INFINITE_UNIVERSES};

    /** If false (default), this module starts a shared thread that calls ProcessAll() every 23 ms.
        If true, no thread is started and the application must call ProcessAll() at its DMX rate,
        usually 23 ms. */
    bool manually_process_source{false};

    /** Create an empty, invalid data structure by default. */
    Settings() = default;
    Settings(const etcpal::Uuid& new_cid, const std::string& new_name);

    bool IsValid() const;
  };

  /**
   * @ingroup sacn_source_cpp
   * @brief A set of configuration settings for a new universe on a source.
   */
  struct UniverseSettings
  {
    /********* Required values **********/

    /** The universe number. At this time, only values from 1 - 63999 are accepted.
        You cannot have a source send more than one stream of values to a single universe. */
    uint16_t universe{0};
    /** The buffer of up to 512 dmx values that will be sent each tick.
        This pointer may not be NULL. The memory is owned by the application, and should not
        be destroyed until after the universe is deleted on this source. */
    const uint8_t* values_buffer{nullptr};
    /** The size of values_buffer. */
    size_t num_values{0};

    /********* Optional values **********/

    /** The sACN universe priority that is sent in each packet. This is only allowed to be from 0 - 200. Defaults to 100. */
    uint8_t priority{100};
    /** The (optional) buffer of up to 512 per-address priorities that will be sent each tick.
        If this is nil, only the universe priority will be used.
        If non-nil, this buffer is evaluated each tick.  Changes to and from 0 ("don't care") cause appropriate
        sacn packets over time to take and give control of those DMX values as defined in the per-address priority
        specification.
        The memory is owned by the application, and should not be destroyed until after the universe is
        deleted on this source. The size of this buffer must match the size of values_buffer.  */
    const uint8_t* priorities_buffer{nullptr};

    /** If true, this sACN source will send preview data. Defaults to false. */
    bool send_preview{false};

    /** If true, this sACN source will only send unicast traffic on this universe. Defaults to false. */
    bool send_unicast_only{false};

    /** If non-zero, this is the synchronization universe used to synchronize the sACN output. Defaults to 0. */
    uint16_t sync_universe{0};

    /** Create an empty, invalid data structure by default. */
    UniverseSettings() = default;
    UniverseSettings(uint16_t universe_id, const uint8_t* new_values_buffer, size_t new_values_size);

    bool IsValid() const;
  };


  Source() = default;
  Source(const Source& other) = delete;
  Source& operator=(const Source& other) = delete;
  Source(Source&& other) = default;             /**< Move a source instance. */
  Source& operator=(Source&& other) = default;  /**< Move a source instance. */

  etcpal::Error Startup(const Settings& settings, std::vector<SacnMcastInterface>& netints);
  void Shutdown();

  etcpal::Error ChangeName(const std::string& new_name);

  etcpal::Error AddUniverse(const UniverseSettings& settings);
  void RemoveUniverse(uint16_t universe);

  etcpal::Error AddUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest);
  void RemoveUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest);

  etcpal::Error ChangePriority(uint16_t universe, uint8_t new_priority);
  etcpal::Error ChangePreviewFlag(uint16_t universe, bool new_preview_flag);
  etcpal::Error ChangeSynchronizationUniverse(uint16_t universe, uint16_t new_sync_universe);

  etcpal::Error SendNow(uint16_t universe, uint8_t start_code, const uint8_t* buffer, size_t buflen);
  etcpal::Error SendSynchronization(uint16_t universe);

  void SetDirty(uint16_t universe);
  void SetDirty(const std::vector<uint16_t>& universes);
  void SetDirtyAndForceSync(uint16_t universe);

  etcpal::Error ResetNetworking(std::vector<SacnMcastInterface>& netints);

  constexpr Handle handle() const;

  static int ProcessAll();

private:
  SacnSourceConfig TranslateConfig(const Settings& settings);
  SacnSourceUniverseConfig TranslateUniverseConfig(const UniverseSettings& settings);

  Handle handle_{kInvalidHandle};
};

/**
 * @brief Create a Source Settings instance by passing the required members explicitly.
 *
 * Optional members can be modified directly in the struct.
 */
inline Source::Settings::Settings(const etcpal::Uuid& new_cid, const std::string& new_name)
    : cid(new_cid), name(new_name)
{
}

/**
 * Determine whether a Source Settings instance contains valid data for sACN operation.
 */
inline bool Source::Settings::IsValid() const
{
  return !cid.IsNull();
}

/**
 * @brief Create a Universe Settings instance by passing the required members explicitly.
 *
 * Optional members can be modified directly in the struct.
 */
inline Source::UniverseSettings::UniverseSettings(uint16_t universe_id, const uint8_t* new_values_buffer,
                                                    size_t new_values_size)
    : universe(universe_id), values_buffer(new_values_buffer), num_values(new_values_size)
{
}

/**
 * Determine whether a Universe Settings instance contains valid data for sACN operation.
 */
inline bool Source::UniverseSettings::IsValid() const
{
  return (universe > 0) && values_buffer && (num_values > 0);
}

/**
 * @brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source, but no data is sent until AddUniverse() and SetDirty() is called.
 *
 * Note that a source is considered as successfully created if it is able to successfully use any of the
 * network interfaces listed in the passed in configuration.  This will only return #kEtcPalErrNoNetints
 * if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the sACN source to be created.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Source successfully created.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate an additional source.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::Startup(const Settings& settings, std::vector<SacnMcastInterface>& netints)
{
  SacnSourceConfig config = TranslateConfig(settings);

  if (netints.empty())
    return sacn_source_create(&config, &handle_, NULL, 0);

  return sacn_source_create(&config, &handle_, netints.data(), netints.size());
}

/**
 * @brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs
 * on a call to ProcessAll() after an additional three packets have been sent with the
 * "Stream_Terminated" option set. The source will also stop transmitting sACN universe discovery packets.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to AddUniverse().
 */
inline void Source::Shutdown()
{
  sacn_source_destroy(handle_);
  handle_ = kInvalidHandle;
}

/**
 * @brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the
 * packet for use in displaying the identity of a source to a user." Only up to
 * #SACN_SOURCE_NAME_MAX_LEN characters will be used.
 *
 * @param[in] new_name New name to use for this universe.
 * @return #kEtcPalErrOk: Name set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ChangeName(const std::string& new_name)
{
  return sacn_source_change_name(handle_, new_name.c_str());
}

/**
 * @brief Add a universe to an sACN source.
 *
 * Adds a universe to a source.
 * After this call completes, the applicaton must call SetDirty() to mark it ready for processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe
 * Discovery packets.

 * @param[in] config Configuration parameters for the universe to be added.
 * @return #kEtcPalErrOk: Universe successfully added.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: Universe given was already added to this source.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::AddUniverse(const UniverseSettings& settings)
{
  SacnSourceUniverseConfig config = TranslateUniverseConfig(settings);
  return sacn_source_add_universe(handle_, &config);
}

/**
 * @brief Remove a universe from a source.
 *
 * This queues the source for removal. The destruction actually occurs
 * on a call to ProcessAll() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * The source will also stop transmitting sACN universe discovery packets for that universe.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to AddUniverse().
 *
 * @param[in] universe Universe to remove.
 */
inline void Source::RemoveUniverse(uint16_t universe)
{
  sacn_source_remove_universe(handle_, universe);
}

/**
 * @brief Add a unicast destination for a source's universe.
 *
 * Adds a unicast destination for a source's universe.
 * After this call completes, the applicaton must call SetDirty() to mark it ready for processing.

 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.
 * @return #kEtcPalErrOk: Address added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::AddUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest)
{
  return sacn_source_add_unicast_destination(handle_, universe, &dest.get());
}

/**
 * @brief Remove a unicast destination on a source's universe.
 *
 * This queues the address for removal. The removal actually occurs
 * on a call to ProcessAll() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  Must match the address passed to AddUnicastDestination().
 */
inline void Source::RemoveUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest)
{
  sacn_source_remove_unicast_destination(handle_, universe, &dest.get());
}

/**
 * @brief Change the priority of a universe on a sACN source.
 *
 * @param[in] universe Universe to change.
 * @param[in] new_priority New priority of the data sent from this source. Valid range is 0 to 200,
 *                         inclusive.
 * @return #kEtcPalErrOk: Priority set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ChangePriority(uint16_t universe, uint8_t new_priority)
{
  return sacn_source_change_priority(handle_, universe, new_priority);
}

/**
 * @brief Change the send_preview option on a universe of a sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
 *
 * @param[in] new_preview_flag The new send_preview option.
 * @return #kEtcPalErrOk: send_preview option set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ChangePreviewFlag(uint16_t universe, bool new_preview_flag)
{
  return sacn_source_change_preview_flag(handle_, universe, new_preview_flag);
}

/**
 * @brief Changes the synchronize uinverse for a universe of a sACN source.
 *
 * This will change the synchronization universe used by a sACN universe on the source.
 * If this value is 0, synchronization is turned off for that universe.
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] universe The universe to change.
 * @param[in] new_sync_universe The new synchronization universe to set.
 * @return #kEtcPalErrOk: sync_universe set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ChangeSynchronizationUniverse(uint16_t universe, uint16_t new_sync_universe)
{
  return sacn_source_change_synchronization_universe(handle_, universe, new_sync_universe);
}

/**
 * @brief Immediately sends the provided sACN start code & data.
 *
 * Immediately sends a sACN packet with the provided start code and data.
 * This function is intended for sACN packets that have a startcode other than 0 or 0xdd, since those
 * start codes are taken care of by ProcessAll().
 *
 * @param[in] universe Universe to send on.
 * @param[in] start_code The start code to send.
 * @param[in] buffer The buffer to send.  Must not be NULL.
 * @param[in] buflen The size of buffer.
 * @return #kEtcPalErrOk: Message successfully sent.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::SendNow(uint16_t universe, uint8_t start_code, const uint8_t* buffer, size_t buflen)
{
  return sacn_source_send_now(handle_, universe, start_code, buffer, buflen);
}

/**
 * @brief Immediately sends a synchronization packet for the universe on a source.
 *
 * This will cause an immediate transmission of a synchronization packet for the source/universe.
 * If the universe does not have a synchronization universe configured, this call is ignored.
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] universe Universe to send on.
 * @return #kEtcPalErrOk: Message successfully sent.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::SendSynchronization(uint16_t universe)
{
  return sacn_source_send_synchronization(handle_, universe);
}

/**
 * @brief Indicate that the data in the buffer for this source and universe has changed and
 *        should be sent on the next call to ProcessAll().
 *
 * @param[in] universe Universe to mark as dirty.
 */
inline void Source::SetDirty(uint16_t universe)
{
  sacn_source_set_dirty(handle_, universe);
}

/**
 * @brief Indicate that the data in the buffers for a list of universes on a source  has
 *        changed and should be sent on the next call to ProcessAll().
 *
 * @param[in] universes Vector of universes to mark as dirty.
 */
inline void Source::SetDirty(const std::vector<uint16_t>& universes)
{
  sacn_source_set_list_dirty(handle_, universes.data(), universes.size());
}

/**
 * @brief Like Source::SetDirty, but also sets the force_sync flag on the packet.
 *
 * This function indicates that the data in the buffer for this source and universe has changed,
 * and should be sent on the next call to ProcessAll().  Additionally, the packet
 * to be sent will have its force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to SetDirty().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] universe Universe to mark as dirty.
 */
inline void Source::SetDirtyAndForceSync(uint16_t universe)
{
  sacn_source_set_dirty_and_force_sync(handle_, universe);
}

/**
 * @brief Process created sources and do the actual sending of sACN data on all universes.
 *
 * Note: Unless you created the source with manually_process_source set to true, this will be automatically
 * called by an internal thread of the module. Otherwise, this must be called at the maximum rate
 * at which the application will send sACN.
 *
 * Sends data for universes which have been marked dirty, and sends keep-alive data for universes which 
 * haven't changed. Also destroys sources & universes that have been marked for termination after sending the required
 * three terminated packets.
 *
 * @return Current number of sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and have actually
 *         been destroyed.
 */
inline int Source::ProcessAll()
{
  return sacn_source_process_all();
}

/**
 * @brief Resets the underlying network sockets for the sACN source..
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, all universes on a source are considered to be dirty and have
 * new values and priorities. It's as if the source just started sending values on that universe.
 *
 * If this call fails, the caller must call Shutdown(), because the source may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * operation_succeeded flags are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Source changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ResetNetworking(std::vector<SacnMcastInterface>& netints)
{
  if (netints.empty())
    return sacn_source_reset_networking(handle_, nullptr, 0);

  return sacn_source_reset_networking(handle_, netints.data(), netints.size());
}

/**
 * @brief Get the current handle to the underlying C sacn_source.
 *
 * @return The handle or Source::kInvalidHandle.
 */
inline constexpr Source::Handle Source::handle() const
{
  return handle_;
}

inline SacnSourceConfig Source::TranslateConfig(const Settings& settings)
{
  // clang-format off
  SacnSourceConfig config = {
    settings.cid.get(),
    "",
    settings.universe_count_max,
    settings.manually_process_source,
  };
  // clang-format on

  ETCPAL_MSVC_BEGIN_NO_DEP_WARNINGS();

  // Update the string
  strncpy(config.name, settings.name.c_str(), SACN_SOURCE_NAME_MAX_LEN);
  config.name[SACN_SOURCE_NAME_MAX_LEN - 1] = 0;

  ETCPAL_MSVC_END_NO_DEP_WARNINGS();

  return config;
}

inline SacnSourceUniverseConfig Source::TranslateUniverseConfig(const UniverseSettings& settings)
{
  // clang-format off
  SacnSourceUniverseConfig config = {
    settings.universe, 
    settings.values_buffer,
    settings.num_values,
    settings.priority,
    settings.priorities_buffer,
    settings.send_preview,
    settings.send_unicast_only,
    settings.sync_universe
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_SOURCE_H_
