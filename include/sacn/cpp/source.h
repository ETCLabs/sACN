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

    /** If false (default), this source will be added to a background thread that will send sACN updates at a
        maximum rate of every 23 ms. If true, the source will not be added to the thread and the application
        must call ProcessManual() at its maximum DMX rate, typically 23 ms. */
    bool manually_process_source{false};

    /** What IP networking the source will support. */
    sacn_ip_support_t ip_supported{kSacnIpV4AndIpV6};

    /** The interval at which the source will send keep-alive packets during transmission suppression, in milliseconds.
     */
    int keep_alive_interval{SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT};

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

    /********* Optional values **********/

    /** The sACN universe priority that is sent in each packet. This is only allowed to be from 0 - 200. Defaults to
        100. */
    uint8_t priority{100};

    /** If true, this sACN source will send preview data. Defaults to false. */
    bool send_preview{false};

    /** If true, this sACN source will only send unicast traffic on this universe. Defaults to false. */
    bool send_unicast_only{false};

    /** The initial set of unicast destinations for this universe. This can be changed further by using
        Source::AddUnicastDestination() and Source::RemoveUnicastDestination(). */
    const std::vector<etcpal::IpAddr> unicast_destinations;

    /** If non-zero, this is the synchronization universe used to synchronize the sACN output. Defaults to 0. */
    uint16_t sync_universe{0};

    /** Create an empty, invalid data structure by default. */
    UniverseSettings() = default;
    UniverseSettings(uint16_t universe_id);

    bool IsValid() const;
  };

  Source() = default;
  Source(const Source& other) = delete;
  Source& operator=(const Source& other) = delete;
  Source(Source&& other) = default;            /**< Move a source instance. */
  Source& operator=(Source&& other) = default; /**< Move a source instance. */

  etcpal::Error Startup(const Settings& settings);
  void Shutdown();

  etcpal::Error ChangeName(const std::string& new_name);

  etcpal::Error AddUniverse(const UniverseSettings& settings);
  etcpal::Error AddUniverse(const UniverseSettings& settings, std::vector<SacnMcastInterface>& netints);
  void RemoveUniverse(uint16_t universe);
  std::vector<uint16_t> GetUniverses();

  etcpal::Error AddUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest);
  void RemoveUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest);
  std::vector<etcpal::IpAddr> GetUnicastDestinations(uint16_t universe);

  etcpal::Error ChangePriority(uint16_t universe, uint8_t new_priority);
  etcpal::Error ChangePreviewFlag(uint16_t universe, bool new_preview_flag);
  etcpal::Error ChangeSynchronizationUniverse(uint16_t universe, uint16_t new_sync_universe);

  etcpal::Error SendNow(uint16_t universe, uint8_t start_code, const uint8_t* buffer, size_t buflen);
  etcpal::Error SendSynchronization(uint16_t universe);

  void UpdateValues(uint16_t universe, const uint8_t* new_values, size_t new_values_size);
  void UpdateValues(uint16_t universe, const uint8_t* new_values, size_t new_values_size, const uint8_t* new_priorities,
                    size_t new_priorities_size);
  void UpdateValuesAndForceSync(uint16_t universe, const uint8_t* new_values, size_t new_values_size);
  void UpdateValuesAndForceSync(uint16_t universe, const uint8_t* new_values, size_t new_values_size,
                                const uint8_t* new_priorities, size_t new_priorities_size);

  etcpal::Error ResetNetworking(uint16_t universe);
  etcpal::Error ResetNetworking(uint16_t universe, std::vector<SacnMcastInterface>& netints);

  std::vector<SacnMcastInterface> GetNetworkInterfaces(uint16_t universe);

  constexpr Handle handle() const;

  static int ProcessManual();

private:
  class TranslatedUniverseConfig
  {
  public:
    explicit TranslatedUniverseConfig(const UniverseSettings& settings);
    const SacnSourceUniverseConfig& get() noexcept;

  private:
    std::vector<EtcPalIpAddr> unicast_destinations_;
    SacnSourceUniverseConfig config_;
  };

  SacnSourceConfig TranslateConfig(const Settings& settings);

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
inline Source::UniverseSettings::UniverseSettings(uint16_t universe_id) : universe(universe_id)
{
}

/**
 * Determine whether a Universe Settings instance contains valid data for sACN operation.
 */
inline bool Source::UniverseSettings::IsValid() const
{
  return universe > 0;
}

/**
 * @brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source, but no data is sent until AddUniverse() and a variant of UpdateValues() is
 * called.
 *
 * @param[in] settings Configuration parameters for the sACN source to be created.
 * @return #kEtcPalErrOk: Source successfully created.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate an additional source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::Startup(const Settings& settings)
{
  SacnSourceConfig config = TranslateConfig(settings);
  return sacn_source_create(&config, &handle_);
}

/**
 * @brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs either on the thread or
 * on a call to ProcessManual() after an additional three packets have been sent with the "Stream_Terminated" option
 * set. The source will also stop transmitting sACN universe discovery packets.
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
 * If a universe is transmitting NULL start code or PAP data, this function will update the outgoing packets, and
 * reset the logic that slows down packet transmission due to inactivity.
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
 * @brief Add a universe to an sACN source, which will use all network interfaces.
 *
 * Adds a universe to a source. All network interfaces will be used.
 * After this call completes, the applicaton must call a variant of UpdateValues() to mark it ready for processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe
 * Discovery packets.
 *
 * Note that a universe is considered as successfully added if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] settings Configuration parameters for the universe to be added.
 * @return #kEtcPalErrOk: Universe successfully added.
 * @return #kEtcPalErrNoNetints: None of the system network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: Universe given was already added to this source.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::AddUniverse(const UniverseSettings& settings)
{
  TranslatedUniverseConfig config(settings);
  return sacn_source_add_universe(handle_, &config.get(), nullptr, 0);
}

/**
 * @brief Add a universe to an sACN source, which will use the network interfaces passed in.
 *
 * Adds a universe to a source. Only the network interfaces passed in will be used.
 * After this call completes, the applicaton must call a variant of UpdateValues() to mark it ready for processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe
 * Discovery packets.
 *
 * Note that a universe is considered as successfully added if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.

 * @param[in] settings Configuration parameters for the universe to be added.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Universe successfully added.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: Universe given was already added to this source.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::AddUniverse(const UniverseSettings& settings, std::vector<SacnMcastInterface>& netints)
{
  TranslatedUniverseConfig config(settings);

  if (netints.empty())
    return sacn_source_add_universe(handle_, &config.get(), nullptr, 0);

  return sacn_source_add_universe(handle_, &config.get(), netints.data(), netints.size());
}

/**
 * @brief Remove a universe from a source.
 *
 * This queues the universe for removal. The destruction actually occurs either on the thread or on a call to
 * ProcessManual() after an additional three packets have been sent with the "Stream_Terminated" option set.
 *
 * The source will also stop transmitting sACN universe discovery packets for that universe.
 *
 * @param[in] universe Universe to remove.
 */
inline void Source::RemoveUniverse(uint16_t universe)
{
  sacn_source_remove_universe(handle_, universe);
}

/**
 * @brief Obtain a vector of universes this source is transmitting on.
 *
 * @return A vector of universes the source is transmitting on.
 */
inline std::vector<uint16_t> Source::GetUniverses()
{
  // This uses a guessing algorithm with a while loop to avoid race conditions.
  std::vector<uint16_t> universes;
  size_t size_guess = 4u;
  size_t num_universes = 0u;

  do
  {
    universes.resize(size_guess);
    num_universes = sacn_source_get_universes(handle_, universes.data(), universes.size());
    size_guess = num_universes + 4u;
  } while (num_universes > universes.size());

  universes.resize(num_universes);
  return universes;
}

/**
 * @brief Add a unicast destination for a source's universe.
 *
 * Adds a unicast destination for a source's universe.
 * After this call completes, the applicaton must call a variant of UpdateValues() to mark it ready for processing.
 *
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.
 * @return #kEtcPalErrOk: Address added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrExists: The unicast destination was already added to this universe on this source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::AddUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest)
{
  return sacn_source_add_unicast_destination(handle_, universe, &dest.get());
}

/**
 * @brief Remove a unicast destination on a source's universe.
 *
 * This queues the address for removal. The removal actually occurs either on the thread or on a call to ProcessManual()
 * after an additional three packets have been sent with the "Stream_Terminated" option set.
 *
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  Must match the address passed to AddUnicastDestination().
 */
inline void Source::RemoveUnicastDestination(uint16_t universe, const etcpal::IpAddr& dest)
{
  sacn_source_remove_unicast_destination(handle_, universe, &dest.get());
}

/**
 * @brief Obtain a vector of unicast destinations to which this source is transmitting a universe.
 *
 * @param[in] universe The universe for which to obtain the list of unicast destinations.
 * @return A vector of unicast destinations the source is transmitting on for the given universe.
 */
inline std::vector<etcpal::IpAddr> Source::GetUnicastDestinations(uint16_t universe)
{
  // This uses a guessing algorithm with a while loop to avoid race conditions.
  std::vector<EtcPalIpAddr> destinations;
  size_t size_guess = 4u;
  size_t num_destinations = 0u;

  do
  {
    destinations.resize(size_guess);
    num_destinations = sacn_source_get_unicast_destinations(handle_, universe, destinations.data(), destinations.size());
    size_guess = num_destinations + 4u;
  } while (num_destinations > destinations.size());

  destinations.resize(num_destinations);

  // Convert vector<EtcPalIpAddr> to vector<etcpal::IpAddr>.
  std::vector<etcpal::IpAddr> result;
  if (!destinations.empty())
  {
    result.reserve(destinations.size());
    std::transform(destinations.begin(), destinations.end(), std::back_inserter(result),
                   [](const EtcPalIpAddr& dest) { return etcpal::IpAddr(dest); });
  }

  return result;
}

/**
 * @brief Change the priority of a universe on a sACN source.
 *
 * If this universe is transmitting NULL start code or PAP data, this function will update the outgoing packets, and
 * reset the logic that slows down packet transmission due to inactivity.
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
 * If this universe is transmitting NULL start code or PAP data, this function will update the outgoing packets, and
 * reset the logic that slows down packet transmission due to inactivity.
 *
 * @param[in] universe The universe to change.
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
 * If this universe is transmitting NULL start code or PAP data, this function will update the outgoing packets, and
 * reset the logic that slows down packet transmission due to inactivity.
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
 * This function is intended for sACN packets that have a startcode other than 0 or 0xdd, since those start codes are
 * taken care of by either the thread or ProcessManual().
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
 * @brief Indicate that a new synchronization packet should be sent on the given synchronization universe.
 *
 * This will cause the transmission of a synchronization packet for the source on the given synchronization universe.
 *
 * TODO: At this time, synchronization is not supported by this library, so this function is not implemented.
 *
 * @param[in] sync_universe The synchronization universe to send on.
 * @return #kEtcPalErrOk: Message successfully sent.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::SendSynchronization(uint16_t sync_universe)
{
  return sacn_source_send_synchronization(handle_, sync_universe);
}

/**
 * @brief Copies the universe's dmx values into the packet to be sent on the next threaded or manual update.
 *
 * This function will update the outgoing packet values, and reset the logic that slows down packet transmission due to
 * inactivity.
 *
 * When you don't have per-address priority changes to make, use this function. Otherwise, use
 * the version of UpdateValues() that takes a per-address priority buffer.
 *
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 */
inline void Source::UpdateValues(uint16_t universe, const uint8_t* new_values, size_t new_values_size)
{
  sacn_source_update_values(handle_, universe, new_values, new_values_size);
}

/**
 * @brief Copies the universe's dmx values and per-address priorities into packets that are sent on the next threaded or
 * manual update.
 *
 * This function will update the outgoing packet values for both DMX and per-address priority data, and reset the logic
 * that slows down packet transmission due to inactivity.
 *
 * Per-address priority support has specific rules about when to send value changes vs. pap changes.  These rules are
 * documented in https://etclabs.github.io/sACN/docs/head/per_address_priority.html, and are triggered by the use of
 * this function. Changing per-address priorities to and from "don't care", changing the size of the priorities array,
 * or passing in NULL/non-NULL for the priorities will cause this library to do the necessary tasks to "take control" or
 * "release control" of the corresponding DMX values.
 *
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities.
 */
inline void Source::UpdateValues(uint16_t universe, const uint8_t* new_values, size_t new_values_size,
                                 const uint8_t* new_priorities, size_t new_priorities_size)
{
  sacn_source_update_values_and_pap(handle_, universe, new_values, new_values_size, new_priorities,
                                    new_priorities_size);
}

/**
 * @brief Like UpdateValues(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next threaded or manual update, and will reset
 * the logic that slows down packet transmission due to inactivity. Additionally, the packet to be sent will have its
 * force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to UpdateValues().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 */
inline void Source::UpdateValuesAndForceSync(uint16_t universe, const uint8_t* new_values, size_t new_values_size)
{
  sacn_source_update_values_and_force_sync(handle_, universe, new_values, new_values_size);
}

/**
 * @brief Like UpdateValues(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next threaded or manual update, and will reset
 * the logic that slows down packet transmission due to inactivity. Additionally, the final packet to be sent by this
 * call will have its force_synchronization option flag set.
 *
 * Per-address priority support has specific rules about when to send value changes vs. pap changes.  These rules are
 * documented in https://etclabs.github.io/sACN/docs/head/per_address_priority.html, and are triggered by the use of
 * this function. Changing per-address priorities to and from "don't care", changing the size of the priorities array,
 * or passing in NULL/non-NULL for the priorities will cause this library to do the necessary tasks to "take control" or
 * "release control" of the corresponding DMX values.
 *
 * If no synchronization universe is configured, this function acts like a direct call to
 * sacn_source_update_values_and_pap().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities.
 */
inline void Source::UpdateValuesAndForceSync(uint16_t universe, const uint8_t* new_values, size_t new_values_size,
                                             const uint8_t* new_priorities, size_t new_priorities_size)
{
  sacn_source_update_values_and_pap_and_force_sync(handle_, universe, new_values, new_values_size, new_priorities,
                                                   new_priorities_size);
}

/**
 * @brief Trigger the transmision of sACN packets for all universes of sources that were created with
 * manually_process_source set to true.
 *
 * Note: Unless you created the source with manually_process_source set to true, similar functionality will be
 * automatically called by an internal thread of the module. Otherwise, this must be called at the maximum rate
 * at which the application will send sACN.
 *
 * Sends the current data for universes which have been updated, and sends keep-alive data for universes which
 * haven't been updated. Also destroys sources & universes that have been marked for termination after sending the
 * required three terminated packets.
 *
 * @return Current number of sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and actually
 *         been destroyed.
 */
inline int Source::ProcessManual()
{
  return sacn_source_process_manual();
}

/**
 * @brief Resets the underlying network sockets for a universe.
 *
 * This is the overload of ResetNetworking that uses all network interfaces.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the universe is considered to be updated and have new values and priorities.
 * It's as if the source just started sending values on that universe.
 *
 * If this call fails, the caller must call Shutdown(), because the source may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] universe Universe to reset network interfaces for.
 * @return #kEtcPalErrOk: Source changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ResetNetworking(uint16_t universe)
{
  std::vector<SacnMcastInterface> netints;
  return ResetNetworking(universe, netints);
}

/**
 * @brief Resets the underlying network sockets for a universe.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the universe is considered to be updated and have new values and priorities.
 * It's as if the source just started sending values on that universe.
 *
 * If this call fails, the caller must call Shutdown(), because the source may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] universe Universe to reset netowrk interfaces for.
 * @param[in, out] netints Optional. If !empty, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If empty, all available interfaces are tried and this vector isn't modified.
 * @return #kEtcPalErrOk: Source changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or a network interface ID given was not
 * found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error Source::ResetNetworking(uint16_t universe, std::vector<SacnMcastInterface>& netints)
{
  if (netints.empty())
    return sacn_source_reset_networking(handle_, universe, nullptr, 0);

  return sacn_source_reset_networking(handle_, universe, netints.data(), netints.size());
}

/**
 * @brief Obtain the statuses of a universe's network interfaces.
 *
 * @param[in] universe The universe for which to obtain the list of network interfaces.
 * @return A vector of the universe's network interfaces and their statuses.
 */
inline std::vector<SacnMcastInterface> Source::GetNetworkInterfaces(uint16_t universe)
{
  // This uses a guessing algorithm with a while loop to avoid race conditions.
  std::vector<SacnMcastInterface> netints;
  size_t size_guess = 4u;
  size_t num_netints = 0u;

  do
  {
    netints.resize(size_guess);
    num_netints = sacn_source_get_network_interfaces(handle_, universe, netints.data(), netints.size());
    size_guess = num_netints + 4u;
  } while (num_netints > netints.size());

  netints.resize(num_netints);
  return netints;
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
    settings.name.c_str(),
    settings.universe_count_max,
    settings.manually_process_source,
    settings.ip_supported,
    settings.keep_alive_interval
  };
  // clang-format on

  return config;
}

inline const SacnSourceUniverseConfig& Source::TranslatedUniverseConfig::get() noexcept
{
  if (!unicast_destinations_.empty())
  {
    config_.unicast_destinations = unicast_destinations_.data();
    config_.num_unicast_destinations = unicast_destinations_.size();
  }

  return config_;
}

// clang-format off
inline Source::TranslatedUniverseConfig::TranslatedUniverseConfig(const UniverseSettings& settings)
    : config_{
        settings.universe,
        settings.priority,
        settings.send_preview,
        settings.send_unicast_only,
        nullptr,
        0,
        settings.sync_universe
      }
{
  // clang-format on

  if (!settings.unicast_destinations.empty())
  {
    unicast_destinations_.reserve(settings.unicast_destinations.size());
    std::transform(settings.unicast_destinations.begin(), settings.unicast_destinations.end(),
                   std::back_inserter(unicast_destinations_), [](const etcpal::IpAddr& dest) { return dest.get(); });
  }
}

};  // namespace sacn

#endif  // SACN_CPP_SOURCE_H_
