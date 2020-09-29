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

#ifndef SACN_CPP_DMX_MERGER_H_
#define SACN_CPP_DMX_MERGER_H_

/// @file sacn/cpp/dmx_merger.h
/// @brief C++ wrapper for the sACN DMX Merger API

#include "sacn/dmx_merger.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"

/// @defgroup sacn_dmx_merger_cpp sACN DMX Merger API
/// @ingroup sacn_cpp_api
/// @brief A C++ wrapper for the sACN DMX Merger API

namespace sacn
{
/// @ingroup sacn_dmx_merger_cpp
/// @brief An instance of sACN DMX Merger functionality.
/// 
/// This class instantiates software mergers for buffers containing DMX512-A start code 0 packets.
/// It also uses buffers containing DMX512-A start code 0xdd packets to support per-address priority.
/// 
/// While this class is used to easily merge the outputs from the sACN Receiver API, it can also be used
/// to merge your own DMX sources together, even in combination with the sources received via sACN.
/// 
/// When asked to calculate the merge, the merger will evaluate the current source
/// buffers and update two result buffers:
///  - 512 bytes for the merged data values (i.e. "winning level").  These are calculated by using
///     a Highest-Level-Takes-Precedence(HTP) algorithm for all sources that share the highest
///     per-address priority.
///  - 512 source identifiers (i.e. "winning source") to indicate which source was considered the
///     source of the merged data value, or that no source currently owns this address.
/// 
/// Usage:
/// @code
/// // These buffers are updated on each merger call with the merge results.
/// // They must be valid as long as the merger is using them.
/// uint8_t slots[DMX_ADDRESS_COUNT];
/// sacn_source_id_t slot_owners[DMX_ADDRESS_COUNT];
/// 
/// // Merger configuration used for the initialization of each merger:
/// sacn::DmxMerger::Settings settings(slots, slot_owners);
/// 
/// // A merger provides a handle for each of its sources. Source CIDs are tracked as well.
/// sacn_source_id_t source_1_handle, source_2_handle;
/// etcpal::Uuid source_1_cid, source_2_cid;
/// // Initialize CIDs here...
/// 
/// // Initialize a merger and two sources, getting the source handles in return.
/// sacn::DmxMerger merger;
/// merger.Startup(settings);
/// 
/// // Make sure to check/handle error cases (this is omitted in this example).
/// source_1_handle = merger.AddSource(source_1_cid).value();
/// source_2_handle = merger.AddSource(source_2_cid).value();
/// 
/// // Input data for merging:
/// uint8_t levels[DMX_ADDRESS_COUNT];
/// uint8_t paps[DMX_ADDRESS_COUNT];
/// uint8_t universe_priority;
/// // Initialize levels, paps, and universe_priority here...
/// 
/// // Levels and PAPs can be merged separately:
/// merger.UpdateSourceData(source_1_handle, universe_priority, levels, DMX_ADDRESS_COUNT);
/// merger.UpdateSourceData(source_1_handle, universe_priority, nullptr, 0, paps, DMX_ADDRESS_COUNT);
/// 
/// // Or together in one call:
/// merger.UpdateSourceData(source_2_handle, universe_priority, levels, DMX_ADDRESS_COUNT, paps, DMX_ADDRESS_COUNT);
/// 
/// // Or, if this is within a sACN receiver callback, use UpdateSourceDataFromSacn:
/// SacnHeaderData header;
/// uint8_t pdata[DMX_ADDRESS_COUNT];
/// // Assuming header and pdata are initialized.
///
/// merger.UpdateSourceDataFromSacn(header, pdata);
/// 
/// // PAP can also be removed. Here, source 1 reverts to universe_priority:
/// merger.StopSourcePerAddressPriority(source_1_handle);
/// 
/// // The read-only state of each source can be obtained as well.
/// const SacnDmxMergerSource* source_1_state = merger.GetSourceInfo(source_1_handle);
/// const SacnDmxMergerSource* source_2_state = merger.GetSourceInfo(source_2_handle);
/// 
/// // Do something with the merge results (slots and slot_owners)...
/// 
/// // Sources can be removed individually:
/// merger.RemoveSource(source_1_handle);
/// merger.RemoveSource(source_2_handle);
/// 
/// // However, when each merger is shut down, all of its sources are removed along with it:
/// merger.Shutdown();
/// @endcode
class DmxMerger
{
public:
  /// A handle type used by the sACN library to identify merger instances.
  using Handle = sacn_dmx_merger_t;
  /// An invalid Handle value.
  static constexpr Handle kInvalidHandle = SACN_DMX_MERGER_INVALID;

  /// @ingroup sacn_dmx_merger_cpp
  /// @brief A set of configuration settings that a merger needs to initialize.
  struct Settings
  {
    /********* Required values **********/

    /*! Buffer of #DMX_ADDRESS_COUNT levels that this library keeps up to date as it merges.
        Memory is owned by the application.*/
    uint8_t* slots{nullptr};

    /*! Buffer of #DMX_ADDRESS_COUNT source IDs that indicate the current winner of the merge for
        that slot, or #DMX_MERGER_SOURCE_INVALID to indicate that no source is providing values for that slot.
        You can use SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners, slot_index) if you don't want to look at the
        slot_owners directly.
        Memory is owned by the application.*/
    sacn_source_id_t* slot_owners{nullptr};

    /********* Optional values **********/

    size_t source_count_max{SACN_RECEIVER_INFINITE_SOURCES};  ///< The maximum number of sources this universe will
                                                              ///< listen to when using dynamic memory.

    /// Create an empty, invalid data structure by default.
    Settings() = default;
    Settings(uint8_t* slots_ptr, sacn_source_id_t* slot_owners_ptr);

    bool IsValid() const;
  };

  DmxMerger() = default;
  DmxMerger(const DmxMerger& other) = delete;
  DmxMerger& operator=(const DmxMerger& other) = delete;
  DmxMerger(DmxMerger&& other) = default;             ///< Move a dmx merger instance.
  DmxMerger& operator=(DmxMerger&& other) = default;  ///< Move a dmx merger instance.

  etcpal::Error Startup(const Settings& settings);
  void Shutdown();

  etcpal::Expected<sacn_source_id_t> AddSource(const etcpal::Uuid& source_cid);
  etcpal::Error RemoveSource(sacn_source_id_t source);
  etcpal::Expected<sacn_source_id_t> GetSourceId(const etcpal::Uuid& source_cid) const;
  const SacnDmxMergerSource* GetSourceInfo(sacn_source_id_t source) const;
  etcpal::Error UpdateSourceData(sacn_source_id_t source, uint8_t priority, const uint8_t* new_values,
                                 size_t new_values_count, const uint8_t* address_priorities = nullptr,
                                 size_t address_priorities_count = 0);
  etcpal::Error UpdateSourceDataFromSacn(const SacnHeaderData& header, const uint8_t* pdata);
  etcpal::Error StopSourcePerAddressPriority(sacn_source_id_t source);

  constexpr Handle handle() const;

private:
  SacnDmxMergerConfig TranslateConfig(const Settings& settings);

  Handle handle_{kInvalidHandle};
};

/// @brief Create a DmxMerger Settings instance by passing the required members explicitly.
///
/// Optional members can be modified directly in the struct.
inline DmxMerger::Settings::Settings(uint8_t* slots_ptr, sacn_source_id_t* slot_owners_ptr)
    : slots(slots_ptr), slot_owners(slot_owners_ptr)
{
}

/// Determine whether a DmxMerger Settings instance contains valid data for sACN operation.
inline bool DmxMerger::Settings::IsValid() const
{
  return (slots && slot_owners);
}

/*!
 * \brief Create a new merger instance.
 *
 * Creates a new merger that uses the passed in config data.  The application owns all buffers
 * in the config, so be sure to call Shutdown() before destroying the buffers.
 *
 * \param[in] config Configuration parameters for the DMX merger to be created.
 * \return #kEtcPalErrOk: Merger created successful.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this merger, or maximum number of mergers has been reached.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::Startup(const Settings& settings)
{
  SacnDmxMergerConfig config = TranslateConfig(settings);
  return sacn_dmx_merger_create(&config, &handle_);
}

/*!
 * \brief Destroy a merger instance.
 *
 * Tears down the merger and cleans up its resources.
 *
 * \return #kEtcPalErrOk: Merger destroyed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline void DmxMerger::Shutdown()
{
  sacn_dmx_merger_destroy(handle_);
  handle_ = kInvalidHandle;
}

/*!
 * \brief Adds a new source to the merger.
 *
 * Adds a new source to the merger, if the maximum number of sources hasn't been reached.
 * The returned source id is used for two purposes:
 *   - It is the handle for calls that need to access the source data.
 *   - It is the source identifer that is put into the slot_owners buffer that was passed
 *     in the DmxMergerUniverseConfig structure when creating the merger.
 *
 * \param[in] source_cid The sACN CID of the source.
 * \return The successfully added source_id.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this source, or the max number of sources has been reached.
 * \return #kEtcPalErrExists: the source at that cid was already added.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Expected<sacn_source_id_t> DmxMerger::AddSource(const etcpal::Uuid& source_cid)
{
  sacn_source_id_t result = SACN_DMX_MERGER_SOURCE_INVALID;
  etcpal_error_t err = sacn_dmx_merger_add_source(handle_, &source_cid.get(), &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/*!
 * \brief Removes a source from the merger.
 *
 * Removes the source from the merger.  This causes the merger to recalculate the outputs.
 *
 * \param[in] source The id of the source to remove.
 * \return #kEtcPalErrOk: Source removed successfully.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::RemoveSource(sacn_source_id_t source)
{
  return sacn_dmx_merger_remove_source(handle_, source);
}

/*!
 * \brief Returns the source id for that source cid.
 *
 * \param[in] source_cid The UUID of the source CID.
 * \return On success this will be the source ID, otherwise kEtcPalErrInvalid.
 */
inline etcpal::Expected<sacn_source_id_t> DmxMerger::GetSourceId(const etcpal::Uuid& source_cid) const
{
  sacn_source_id_t result = sacn_dmx_merger_get_id(handle_, &source_cid.get());
  if (result != SACN_DMX_MERGER_SOURCE_INVALID)
    return result;
  return kEtcPalErrInvalid;
}

/*!
 * \brief Gets a read-only view of the source data.
 *
 * Looks up the source data and returns a pointer to the data or nullptr if it doesn't exist.
 * This pointer is owned by the library, and must not be modified by the application.
 * The pointer will only be valid until the source or merger is removed.
 *
 * \param[in] source The id of the source.
 * \return The reference to the source data, otherwise kEtcPalErrInvalid.
 */
inline const SacnDmxMergerSource* DmxMerger::GetSourceInfo(sacn_source_id_t source) const
{
  return sacn_dmx_merger_get_source(handle_, source);
}

/*!
 * \brief Updates the source data and recalculate outputs.
 *
 * The direct method to change source data.  This causes the merger to recalculate the outputs.
 * If you are processing sACN packets, you may prefer UpdateSourceDataFromSacn().
 *
 * \param[in] source The id of the source to modify.
 * \param[in] priority The universe-level priority of the source.
 * \param[in] new_values The new DMX values to be copied in. This must be nullptr if the source is not updating DMX
 * data.
 * \param[in] new_values_count The length of new_values. Must be 0 if the source is not updating DMX data.
 * \param[in] address_priorities The per-address priority values to be copied in.  This must be nullptr if the source is
 * not updating per-address priority data.
 * \param[in] address_priorities_count The length of address_priorities.  Must be 0 if the source is not updating
 * per-address priority data.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::UpdateSourceData(sacn_source_id_t source, uint8_t priority, const uint8_t* new_values,
                                                 size_t new_values_count, const uint8_t* address_priorities,
                                                 size_t address_priorities_count)
{
  return sacn_dmx_merger_update_source_data(handle_, source, priority, new_values, new_values_count, address_priorities,
                                            address_priorities_count);
}

/*!
 * \brief Updates the source data from a sACN packet and recalculate outputs.
 *
 * Processes data passed from the sACN receiver's SacnUniverseDataCallback() handler.  This causes the merger to
 * recalculate the outputs.
 *
 * \param[in] header The sACN header.
 * \param[in] pdata The sACN data.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid merger, or source CID in the header doesn't match
 * a known source.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::UpdateSourceDataFromSacn(const SacnHeaderData& header, const uint8_t* pdata)
{
  return sacn_dmx_merger_update_source_from_sacn(handle_, &header, pdata);
}

/*!
 * \brief Removes the per-address data from the source and recalculate outputs.
 *
 * Per-address priority data can time out in sACN just like values.
 * This is a convenience function to immediately turn off the per-address priority data for a source and recalculate the
 * outputs.
 *
 * \param[in] source The id of the source to modify.
 * \return #kEtcPalErrOk: Source updated and merge completed.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::StopSourcePerAddressPriority(sacn_source_id_t source)
{
  return sacn_dmx_merger_stop_source_per_address_priority(handle_, source);
}

/*!
 * \brief Get the current handle to the underlying C sacn_receiver.
 *
 * \return The handle or Receiver::kInvalidHandle.
 */
inline constexpr DmxMerger::Handle DmxMerger::handle() const
{
  return handle_;
}

inline SacnDmxMergerConfig DmxMerger::TranslateConfig(const Settings& settings)
{
  // clang-format off
  SacnDmxMergerConfig config = {
    settings.source_count_max,
    settings.slots,
    settings.slot_owners
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_DMX_MERGER_H_
