/******************************************************************************
 * Copyright 2024 ETC Inc.
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

/**
 * @file sacn/cpp/dmx_merger.h
 * @brief C++ wrapper for the sACN DMX Merger API
 */

#include "sacn/cpp/common.h"

#include "sacn/dmx_merger.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/opaque_id.h"

/**
 * @defgroup sacn_dmx_merger_cpp sACN DMX Merger API
 * @ingroup sacn_cpp_api
 * @brief A C++ wrapper for the sACN DMX Merger API
 */

namespace sacn
{
namespace detail
{
class DmxMergerHandleType
{
};
};  // namespace detail

/**
 * @ingroup sacn_dmx_merger_cpp
 * @brief An instance of sACN DMX Merger functionality; see @ref using_dmx_merger.
 *
 * This class instantiates software mergers for buffers containing DMX512-A start code 0 packets.
 * It also uses buffers containing DMX512-A start code 0xdd packets to support per-address priority.
 *
 * When asked to calculate the merge, the merger will evaluate the current source
 * buffers and update two result buffers:
 *  - 512 bytes for the merged data levels (i.e. "winning level").  These are calculated by using
 *     a Highest-Level-Takes-Precedence(HTP) algorithm for all sources that share the highest
 *     per-address priority.
 *  - 512 source identifiers (i.e. "winning source") to indicate which source was considered the
 *     source of the merged data level, or that no source currently owns this address.
 *
 * See @ref using_dmx_merger for a detailed description of how to use this API.
 */
class DmxMerger
{
public:
  /** A handle type used by the sACN library to identify merger instances. */
  using Handle = etcpal::OpaqueId<detail::DmxMergerHandleType, sacn_dmx_merger_t, SACN_DMX_MERGER_INVALID>;

  /**
   * @ingroup sacn_dmx_merger_cpp
   * @brief A set of configuration settings that a merger needs to initialize.
   */
  struct Settings
  {
    /** This is always required to be non-NULL.
        Buffer of #SACN_DMX_MERGER_MAX_SLOTS levels that this library keeps up to date as it merges.  Slots that are not
       sourced are set to 0. Memory is owned by the application and must remain allocated until the merger is destroyed.
       While this merger exists, the application must not modify this buffer directly!  Doing so would affect the
       results of the merge.*/
    uint8_t* levels{nullptr};

    /** This is only allowed to be NULL if and only if #SACN_DMX_MERGER_DISABLE_INTERNAL_PAP_BUFFER is 0.
        Buffer of #SACN_DMX_MERGER_MAX_SLOTS per-address priorities for each winning slot. This is used if the merge
        results need to be sent over sACN. Otherwise this can just be set to nullptr. If a source with a universe
        priority of 0 wins, that priority is converted to 1. If there is no winner for a slot, then a per-address
        priority of 0 is used to show that there is no source for that slot.
        Memory is owned by the application and must remain allocated until the merger is destroyed.*/
    uint8_t* per_address_priorities{nullptr};

    /** This is allowed to be NULL.
        If the merger output is being transmitted via sACN, this is set to true if per-address-priority packets should
        be transmitted. Otherwise this is set to false. This can be set to nullptr if not needed, which can save some
        performance.*/
    bool* per_address_priorities_active{nullptr};

    /** This is allowed to be NULL.
        This is set to the highest universe priority of the currently winning sources. If the merger's output is
        transmitted by a sACN source, this can be used for the packets' universe priority field. Otherwise this can be
        set to nullptr if not needed.*/
    uint8_t* universe_priority{nullptr};

    /** This is only allowed to be NULL if and only if #SACN_DMX_MERGER_DISABLE_INTERNAL_OWNER_BUFFER is 0.
        Buffer of #SACN_DMX_MERGER_MAX_SLOTS source IDs that indicate the current winner of the merge for that slot, or
        #SACN_DMX_MERGER_SOURCE_INVALID to indicate that there is no winner for that slot. This is used if
        you need to know the source of each slot. If you only need to know whether or not a slot is sourced, set this to
        NULL and use per_address_priorities (which has half the memory footprint) to check if the slot has a priority of
        0 (not sourced).
        Memory is owned by the application and must remain allocated until the merger is destroyed.*/
    sacn_dmx_merger_source_t* owners{nullptr};

    int source_count_max{SACN_RECEIVER_INFINITE_SOURCES}; /**< The maximum number of sources this universe will
                                                                listen to when using dynamic memory. */

    /** Create an empty, invalid data structure by default. */
    Settings() = default;

    /** Initializes merger settings with the levels output pointer. This constructor is not marked explicit on purpose
       so that a Settings instance can be implicitly constructed from a levels output pointer, if that's all the
        application needs.
     */
    Settings(uint8_t* levels_ptr);

    bool IsValid() const;
  };

  DmxMerger()                                  = default;
  DmxMerger(const DmxMerger& other)            = delete;
  DmxMerger& operator=(const DmxMerger& other) = delete;
  DmxMerger(DmxMerger&& other)                 = default; /**< Move a dmx merger instance. */
  DmxMerger& operator=(DmxMerger&& other)      = default; /**< Move a dmx merger instance. */

  etcpal::Error Startup(const Settings& settings);
  void          Shutdown();

  etcpal::Expected<sacn_dmx_merger_source_t> AddSource();
  etcpal::Error                              RemoveSource(sacn_dmx_merger_source_t source);
  const SacnDmxMergerSource*                 GetSourceInfo(sacn_dmx_merger_source_t source) const;

  etcpal::Error UpdateLevels(sacn_dmx_merger_source_t source, const uint8_t* new_levels, size_t new_levels_count);
  etcpal::Error UpdatePap(sacn_dmx_merger_source_t source, const uint8_t* pap, size_t pap_count);
  etcpal::Error UpdateUniversePriority(sacn_dmx_merger_source_t source, uint8_t universe_priority);
  etcpal::Error RemovePap(sacn_dmx_merger_source_t source);

  constexpr Handle handle() const;

private:
  SacnDmxMergerConfig TranslateConfig(const Settings& settings);

  Handle handle_;
};

/**
 * @brief Create a DmxMerger Settings instance by passing the required members explicitly.
 *
 * Optional members can be modified directly in the struct.
 */
inline DmxMerger::Settings::Settings(uint8_t* levels_ptr) : levels(levels_ptr)
{
}

/**
 * Determine whether a DmxMerger Settings instance contains valid data for sACN operation.
 */
inline bool DmxMerger::Settings::IsValid() const
{
  return (levels != nullptr);
}

/**
 * @brief Create a new merger instance.
 *
 * Creates a new merger that uses the passed in config data.  The application owns all buffers
 * in the config, so be sure to call Shutdown() before destroying the buffers.
 *
 * @param[in] settings Configuration parameters for the DMX merger to be created.
 * @return #kEtcPalErrOk: Merger created successful.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this merger, or maximum number of mergers has been reached.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::Startup(const Settings& settings)
{
  SacnDmxMergerConfig config = TranslateConfig(settings);

  sacn_dmx_merger_t c_handle = SACN_DMX_MERGER_INVALID;
  etcpal::Error     result   = sacn_dmx_merger_create(&config, &c_handle);

  handle_.SetValue(c_handle);

  return result;
}

/**
 * @brief Destroy a merger instance.
 *
 * Tears down the merger and cleans up its resources.
 *
 * @return #kEtcPalErrOk: Merger destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline void DmxMerger::Shutdown()
{
  sacn_dmx_merger_destroy(handle_.value());
  handle_.Clear();
}

/**
 * @brief Adds a new source to the merger.
 *
 * Adds a new source to the merger, if the maximum number of sources hasn't been reached.
 * The returned source id is used for two purposes:
 *   - It is the handle for calls that need to access the source data.
 *   - It is the source identifer that is put into the owners buffer that was passed
 *     in the DmxMergerUniverseConfig structure when creating the merger.
 *
 * @return The successfully added source_id.
 * @return #kEtcPalErrInvalid: The merger was not started correctly.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this source, or the max number of sources has been reached.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Expected<sacn_dmx_merger_source_t> DmxMerger::AddSource()
{
  sacn_dmx_merger_source_t result = SACN_DMX_MERGER_SOURCE_INVALID;
  etcpal_error_t           err    = sacn_dmx_merger_add_source(handle_.value(), &result);
  if (err == kEtcPalErrOk)
    return result;
  else
    return err;
}

/**
 * @brief Removes a source from the merger.
 *
 * Removes the source from the merger.  This causes the merger to recalculate the outputs.
 *
 * @param[in] source The id of the source to remove.
 * @return #kEtcPalErrOk: Source removed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::RemoveSource(sacn_dmx_merger_source_t source)
{
  return sacn_dmx_merger_remove_source(handle_.value(), source);
}

/**
 * @brief Gets a read-only view of the source data.
 *
 * Looks up the source data and returns a pointer to the data or nullptr if it doesn't exist.
 * This pointer is owned by the library, and must not be modified by the application.
 * The pointer will only be valid until the source or merger is removed.
 *
 * @param[in] source The id of the source.
 * @return The reference to the source data, otherwise kEtcPalErrInvalid.
 */
inline const SacnDmxMergerSource* DmxMerger::GetSourceInfo(sacn_dmx_merger_source_t source) const
{
  return sacn_dmx_merger_get_source(handle_.value(), source);
}

/**
 * @brief Updates a source's levels and recalculates outputs.
 *
 * This function updates the levels of the specified source, and then triggers the recalculation of each slot. Only
 * slots within the valid level range will ever be factored into the merge. If the level count increased (it is 0
 * initially), previously inputted priorities will be factored into the recalculation for the added slots. However, if
 * the level count decreased, the slots that were lost will be released and will no longer be part of the merge. For
 * each slot, the source will only be included in the merge if it has a priority at that slot. Otherwise the level will
 * be saved for when a priority is eventually inputted.
 *
 * @param[in] source The id of the source to modify.
 * @param[in] new_levels The new DMX levels to be copied in, starting from the first slot.
 * @param[in] new_levels_count The length of new_levels. Only slots within this range will ever be factored into the
 * merge.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::UpdateLevels(sacn_dmx_merger_source_t source,
                                             const uint8_t*           new_levels,
                                             size_t                   new_levels_count)
{
  return sacn_dmx_merger_update_levels(handle_.value(), source, new_levels, new_levels_count);
}

/**
 * @brief Updates a source's per-address priorities (PAP) and recalculates outputs.
 *
 * This function updates the per-address priorities (PAP) of the specified source, and then triggers the recalculation
 * of each slot within the current valid level count (thus no merging will occur if levels haven't been inputted yet).
 * Priorities beyond this count are saved, and eventually merged once levels beyond this count are inputted. For each
 * slot, the source will only be included in the merge if it has a priority at that slot.
 *
 * If PAP is not specified for all levels, then the remaining levels will default to a PAP of 0. To remove PAP for this
 * source and revert to the universe priority, call DmxMerger::RemovePap.
 *
 * @param[in] source The id of the source to modify.
 * @param[in] pap The per-address priorities to be copied in, starting from the first slot.
 * @param[in] pap_count The length of pap.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::UpdatePap(sacn_dmx_merger_source_t source, const uint8_t* pap, size_t pap_count)
{
  return sacn_dmx_merger_update_pap(handle_.value(), source, pap, pap_count);
}

/**
 * @brief Updates a source's universe priority and recalculates outputs.
 *
 * This function updates the universe priority of the specified source, and then triggers the recalculation of each
 * slot within the current valid level count (thus no merging will occur if levels haven't been inputted yet).
 * Priorities for slots beyond this count are saved, and eventually merged once levels beyond this count are inputted.
 * For each slot, the source will only be included in the merge if it has a priority at that slot.
 *
 * If this source currently has per-address priorities (PAP) via DmxMerger::UpdatePap, then the universe priority can
 * have no effect on the merge results until the application calls DmxMerger::RemovePap, at which point the priorities
 * of each slot will revert to the universe priority passed in here.
 *
 * If this source doesn't have PAP, then the universe priority is converted into PAP for each slot. These are the
 * priorities used for the merge. This means a universe priority of 0 will be converted to a PAP of 1.
 *
 * @param[in] source The id of the source to modify.
 * @param[in] universe_priority The universe-level priority of the source.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::UpdateUniversePriority(sacn_dmx_merger_source_t source, uint8_t universe_priority)
{
  return sacn_dmx_merger_update_universe_priority(handle_.value(), source, universe_priority);
}

/**
 * @brief Removes the per-address priority (PAP) data from the source and recalculate outputs.
 *
 * Per-address priority data can time out in sACN just like levels.
 * This is a convenience function to immediately turn off the per-address priority data for a source and recalculate the
 * outputs currently within the valid level count (no merging will occur if levels haven't been inputted yet).
 * Priorities for slots beyond this count will eventually be merged once levels beyond this count are inputted.
 *
 * @param[in] source The id of the source to modify.
 * @return #kEtcPalErrOk: Source updated and merge completed.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or merger.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
inline etcpal::Error DmxMerger::RemovePap(sacn_dmx_merger_source_t source)
{
  return sacn_dmx_merger_remove_pap(handle_.value(), source);
}

/**
 * @brief Get the current handle to the underlying C DMX merger.
 *
 * @return The handle, which will only be valid if the DMX merger has been successfully created using Startup().
 */
inline constexpr DmxMerger::Handle DmxMerger::handle() const
{
  return handle_;
}

inline SacnDmxMergerConfig DmxMerger::TranslateConfig(const Settings& settings)
{
  // clang-format off
  SacnDmxMergerConfig config = {
    settings.levels,
    settings.per_address_priorities,
    settings.per_address_priorities_active,
    settings.universe_priority,
    settings.owners,
    settings.source_count_max
  };
  // clang-format on

  return config;
}

};  // namespace sacn

#endif  // SACN_CPP_DMX_MERGER_H_
