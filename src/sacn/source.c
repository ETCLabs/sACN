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

#include "sacn/source.h"
#include "sacn/private/common.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"
#include "sacn/private/sockets.h"
#include "sacn/private/source.h"
#include "sacn/private/util.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "etcpal/rbtree.h"
#include "etcpal/timer.h"

// Suppress strncpy() warning on Windows/MSVC.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/****************************** Private macros *******************************/

#define SOURCE_THREAD_INTERVAL 23
#define UNIVERSE_DISCOVERY_INTERVAL 10000
#define NUM_PRE_SUPPRESSION_PACKETS 4
#define IS_PART_OF_UNIVERSE_DISCOVERY(universe) (universe->has_null_data && !universe->send_unicast_only)

/**************************** Private variables ******************************/

static IntHandleManager source_handle_mgr;
static bool shutting_down = false;
static etcpal_thread_t source_thread_handle;
static bool thread_initialized = false;

/*********************** Private function prototypes *************************/

static bool source_handle_in_use(int handle_val, void* cookie);

static etcpal_error_t start_tick_thread();
static void stop_tick_thread();

static void source_thread_function(void* arg);

static int process_internal(bool process_manual);
static int process_sources(bool process_manual);
static void process_universe_discovery(SacnSource* source);
static void process_universes(SacnSource* source);
static void process_unicast_dests(SacnSource* source, SacnSourceUniverse* universe);
static bool process_universe_termination(SacnSource* source, SacnSourceUniverse* universe, EtcPalRbIter* universe_iter);
static void process_universe_null_pap_transmission(SacnSource* source, SacnSourceUniverse* universe);
static void send_data_multicast_ipv4_ipv6(const SacnSource* source, SacnSourceUniverse* universe,
                                          const uint8_t* send_buf);
static void send_data_unicast_ipv4_ipv6(const SacnSource* source, SacnSourceUniverse* universe,
                                        const uint8_t* send_buf);
static void increment_sequence_number(SacnSourceUniverse* universe);
static void send_null_data_multicast(const SacnSource* source, SacnSourceUniverse* universe);
static void send_null_data_unicast(const SacnSource* source, SacnSourceUniverse* universe);
static void process_null_sent(SacnSourceUniverse* universe);
#if SACN_ETC_PRIORITY_EXTENSION
static void send_pap_data_multicast(const SacnSource* source, SacnSourceUniverse* universe);
static void send_pap_data_unicast(const SacnSource* source, SacnSourceUniverse* universe);
static void process_pap_sent(SacnSourceUniverse* universe);
#endif
static void send_termination_multicast(const SacnSource* source, SacnSourceUniverse* universe);
static void send_termination_unicast(const SacnSource* source, SacnSourceUniverse* universe,
                                     SacnUnicastDestination* dest);
static void send_universe_discovery(SacnSource* source);
static void send_data_unicast(etcpal_iptype_t ip_type, const uint8_t* send_buf, EtcPalRbTree* dests);
static void send_data_to_single_unicast_dest(etcpal_iptype_t ip_type, const uint8_t* send_buf,
                                             const SacnUnicastDestination* dest);
static int pack_universe_discovery_page(SacnSource* source, EtcPalRbIter* universe_iter, uint8_t page_number);
static void init_send_buf(uint8_t* send_buf, uint8_t start_code, const EtcPalUuid* source_cid, const char* source_name,
                          uint8_t priority, uint16_t universe, uint16_t sync_universe, bool send_preview);
static void update_data(uint8_t* send_buf, const uint8_t* new_data, uint16_t new_data_size, bool force_sync);
static void update_levels(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_levels,
                          size_t new_levels_size, bool force_sync);
#if SACN_ETC_PRIORITY_EXTENSION
static void update_paps(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_priorities,
                        size_t new_priorities_size, bool force_sync);
#endif
static void update_levels_and_or_paps(SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_values,
                                      size_t new_values_size, const uint8_t* new_priorities, size_t new_priorities_size,
                                      bool force_sync);
static void set_source_terminating(SacnSource* source);
static void set_universe_terminating(SacnSourceUniverse* universe);
static void set_unicast_dest_terminating(SacnUnicastDestination* dest);
static void reset_transmission_suppression(const SacnSource* source, SacnSourceUniverse* universe, bool reset_null,
                                           bool reset_pap);
static void set_source_name(SacnSource* source, const char* new_name);
static void set_universe_priority(const SacnSource* source, SacnSourceUniverse* universe, uint8_t priority);
static void set_preview_flag(const SacnSource* source, SacnSourceUniverse* universe, bool preview);

/*************************** Function definitions ****************************/

/* Initialize the sACN Source module. Internal function called from sacn_init(). */
etcpal_error_t sacn_source_init(void)
{
#if SACN_SOURCE_ENABLED
  init_int_handle_manager(&source_handle_mgr, source_handle_in_use, NULL);
#endif

  return kEtcPalErrOk;
}

void sacn_source_deinit(void)
{
  // Shut down the Tick thread...
  bool thread_initted = false;
  if (sacn_lock())
  {
    thread_initted = thread_initialized;
    thread_initialized = false;
    sacn_unlock();
  }

  if (thread_initted)
    stop_tick_thread();
}

/**
 * @brief Initialize an sACN Source Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_config_init(SacnSourceConfig* config)
{
  if (config)
  {
    config->cid = kEtcPalNullUuid;
    config->name = NULL;
    config->universe_count_max = SACN_SOURCE_INFINITE_UNIVERSES;
    config->manually_process_source = false;
    config->ip_supported = kSacnIpV4AndIpV6;
    config->keep_alive_interval = SACN_SOURCE_KEEP_ALIVE_INTERVAL_DEFAULT;
  }
}

/**
 * @brief Initialize an sACN Source Universe Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_universe_config_init(SacnSourceUniverseConfig* config)
{
  if (config)
  {
    config->universe = 0;
    config->priority = 100;
    config->send_preview = false;
    config->send_unicast_only = false;
    config->unicast_destinations = NULL;
    config->num_unicast_destinations = 0;
    config->sync_universe = 0;
  }
}

/**
 * @brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source and begins sending universe discovery packets for it (which will list no
 * universes until start code data begins transmitting). No start code data is sent until sacn_source_add_universe() and
 * a variant of sacn_source_update_values() is called.
 *
 * @param[in] config Configuration parameters for the sACN source to be created. If any of these parameters are invalid,
 * #kEtcPalErrInvalid will be returned. This includes if the source name's length (including the null terminator) is
 * beyond #SACN_SOURCE_NAME_MAX_LEN.
 * @param[out] handle Filled in on success with a handle to the sACN source.
 * @return #kEtcPalErrOk: Source successfully created.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate an additional source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (!config)
      result = kEtcPalErrInvalid;
    else if (ETCPAL_UUID_IS_NULL(&config->cid))
      result = kEtcPalErrInvalid;
    else if (!config->name)
      result = kEtcPalErrInvalid;
    else if (strlen(config->name) > (SACN_SOURCE_NAME_MAX_LEN - 1))  // Max length includes null terminator
      result = kEtcPalErrInvalid;
    else if (config->keep_alive_interval <= 0)
      result = kEtcPalErrInvalid;
    else if (!handle)
      result = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    // If the Tick thread hasn't been started yet, start it if the config isn't manual.
    if (result == kEtcPalErrOk)
    {
      if (!thread_initialized && !config->manually_process_source)
      {
        result = start_tick_thread();

        if (result == kEtcPalErrOk)
          thread_initialized = true;
      }
    }

    // Allocate the source's state.
    SacnSource* source = NULL;
    if (result == kEtcPalErrOk)
    {
      source = add_sacn_source(get_next_int_handle(&source_handle_mgr, -1));

      if (!source)
        result = kEtcPalErrNoMem;
    }

    if (result == kEtcPalErrOk)
    {
      // Initialize the universe discovery send buffer.
      size_t written = 0;
      written += pack_sacn_root_layer(source->universe_discovery_send_buf, SACN_UNIVERSE_DISCOVERY_HEADER_SIZE, true,
                                      &config->cid);
      written +=
          pack_sacn_universe_discovery_framing_layer(&source->universe_discovery_send_buf[written], 0, config->name);
      written += pack_sacn_universe_discovery_layer_header(&source->universe_discovery_send_buf[written], 0, 0, 0);

      // Initialize everything else.
      source->cid = config->cid;
      memcpy(source->name, config->name, strlen(config->name));
      source->terminating = false;
      source->num_active_universes = 0;
      etcpal_timer_start(&source->universe_discovery_timer, UNIVERSE_DISCOVERY_INTERVAL);
      source->process_manually = config->manually_process_source;
      source->ip_supported = config->ip_supported;
      source->keep_alive_interval = config->keep_alive_interval;
      source->universe_count_max = config->universe_count_max;

      // Initialize the handle on success.
      *handle = source->handle;
    }

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the packet for use in
 * displaying the identity of a source to a user." If its length (including the null terminator) is longer than
 * #SACN_SOURCE_NAME_MAX_LEN, then #kEtcPalErrInvalid will be returned.
 *
 * This function will update the packet buffers of all this source's universes with the new name. For each universe that
 * is transmitting NULL start code or PAP data, the logic that slows down packet transmission due to inactivity will be
 * reset.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] new_name New name to use for this source.
 * @return #kEtcPalErrOk: Name set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_SOURCE_INVALID)
      result = kEtcPalErrInvalid;
    else if (!new_name)
      result = kEtcPalErrInvalid;
    else if (strlen(new_name) > (SACN_SOURCE_NAME_MAX_LEN - 1))  // Max length includes null terminator
      result = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    // Look up the source's state.
    SacnSource* source = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_source_state(handle, 0, &source, NULL);

    // Set this source's name.
    if (result == kEtcPalErrOk)
      set_source_name(source, new_name);

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs either on the thread or
 * on a call to sacn_source_process_manual() after an additional three packets have been sent with the
 * "Stream_Terminated" option set. The source will also stop transmitting sACN universe discovery packets.
 *
 * @param[in] handle Handle to the source to destroy.
 */
void sacn_source_destroy(sacn_source_t handle)
{
  // Validate and lock.
#if SACN_SOURCE_ENABLED
  if (sacn_initialized() && (handle != SACN_SOURCE_INVALID) && sacn_lock())
  {
    // Try to find the source's state.
    SacnSource* source = NULL;
    lookup_source_state(handle, 0, &source, NULL);

    // If the source was found, initiate termination.
    if (source)
      set_source_terminating(source);

    sacn_unlock();
  }
#endif
}

/**
 * @brief Add a universe to an sACN source.
 *
 * Adds a universe to a source.
 * After this call completes, the applicaton must call a variant of sacn_source_update_values() to mark it ready for
 * processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe Discovery packets
 * once a variant of sacn_source_update_values() is called.
 *
 * Note that a universe is considered as successfully added if it is able to successfully use any of the
 * network interfaces.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the source to which to add a universe.
 * @param[in] config Configuration parameters for the universe to be added.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and
 * the status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Universe successfully added.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: Universe given was already added to this source.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                        SacnMcastInterface* netints, size_t num_netints)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_SOURCE_INVALID)
      result = kEtcPalErrInvalid;
    else if (!config)
      result = kEtcPalErrInvalid;
    else if (!UNIVERSE_ID_VALID(config->universe) || !UNIVERSE_ID_VALID(config->sync_universe))
      result = kEtcPalErrInvalid;
    else if ((config->num_unicast_destinations > 0) && !config->unicast_destinations)
      result = kEtcPalErrInvalid;
    else
    {
      for (size_t i = 0; i < config->num_unicast_destinations; ++i)
      {
        if (ETCPAL_IP_IS_INVALID(&config->unicast_destinations[i]))
          result = kEtcPalErrInvalid;
      }
    }
  }

  if (sacn_lock())
  {
    // Look up the source's state.
    SacnSource* source = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_source_state(handle, 0, &source, NULL);

    // Confirm that the universe wasn't already added.
    if (result == kEtcPalErrOk)
    {
      SacnSourceUniverse* tmp;
      if (lookup_source_state(handle, config->universe, &source, &tmp) != kEtcPalErrNotFound)
        result = kEtcPalErrExists;
    }

#if SACN_DYNAMIC_MEM
    // Make sure to check against universe_count_max.
    if (result == kEtcPalErrOk)
    {
      if ((source->universe_count_max != SACN_SOURCE_INFINITE_UNIVERSES) &&
          (etcpal_rbtree_size(&source->universes) >= source->universe_count_max))
      {
        result = kEtcPalErrNoMem;  // No room to allocate additional universe.
      }
    }
#endif

    // Allocate the universe's state.
    SacnSourceUniverse* universe = NULL;
    if (result == kEtcPalErrOk)
    {
      universe = add_sacn_source_universe(source, config->universe);

      if (!universe)
        result = kEtcPalErrNoMem;
    }

    // Initialize the universe's state.
    if (result == kEtcPalErrOk)
    {
      universe->terminating = false;
      universe->num_terminations_sent = 0;

      universe->priority = config->priority;
      universe->sync_universe = config->sync_universe;
      universe->send_preview = config->send_preview;
      universe->seq_num = 0;

      universe->null_packets_sent_before_suppression = 0;
      init_send_buf(universe->null_send_buf, 0x00, &source->cid, source->name, config->priority, config->universe,
                    config->sync_universe, config->send_preview);
      universe->has_null_data = false;

#if SACN_ETC_PRIORITY_EXTENSION
      universe->pap_packets_sent_before_suppression = 0;
      init_send_buf(universe->pap_send_buf, 0xDD, &source->cid, source->name, config->priority, config->universe,
                    config->sync_universe, config->send_preview);
      universe->has_pap_data = false;
#endif

      universe->send_unicast_only = config->send_unicast_only;

      for (size_t i = 0; (result == kEtcPalErrOk) && (i < config->num_unicast_destinations); ++i)
      {
        SacnUnicastDestination* dest = add_sacn_unicast_dest(universe, &config->unicast_destinations[i]);
        if (dest)
        {
          dest->terminating = false;
          dest->num_terminations_sent = 0;
        }
        else
        {
          if (lookup_unicast_dest(handle, config->universe, &config->unicast_destinations[i], NULL) == kEtcPalErrOk)
            result = kEtcPalErrOk;  // Duplicates are automatically filtered and not a failure condition.
          else
            result = kEtcPalErrNoMem;
        }
      }
    }

    if (result == kEtcPalErrOk)
      result = sacn_initialize_internal_netints(&universe->netints, &universe->num_netints, netints, num_netints);

    // Update the source's netint tracking.
    for (size_t i = 0; (result == kEtcPalErrOk) && (i < universe->num_netints); ++i)
    {
      SacnSourceNetint* netint = lookup_source_netint(source, &universe->netints[i]);

      if (netint)
      {
        // Update existing netint by incrementing the ref counter.
        ++netint->num_refs;
      }
      else
      {
        // Add a new netint with ref counter = 1.
        netint = add_sacn_source_netint(source, &universe->netints[i]);

        if (netint)
          netint->num_refs = 1;
        else
          result = kEtcPalErrNoMem;
      }
    }

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Remove a universe from a source.
 *
 * This queues the universe for removal. The destruction actually occurs either on the thread or on a call to
 * sacn_source_process_manual() after an additional three packets have been sent with the "Stream_Terminated" option
 * set.
 *
 * The source will also stop transmitting sACN universe discovery packets for that universe.
 *
 * @param[in] handle Handle to the source from which to remove the universe.
 * @param[in] universe Universe to remove.
 */
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe)
{
#if SACN_SOURCE_ENABLED
  if (sacn_lock())
  {
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_state(handle, universe, NULL, &universe_state);

    if (universe_state)
      set_universe_terminating(universe_state);

    sacn_unlock();
  }
#endif
}

/**
 * @brief Obtain a list of a source's universes.
 *
 * @param[in] handle Handle to the source for which to obtain the list of universes.
 * @param[out] universes A pointer to an application-owned array where the universe list will be written.
 * @param[in] universes_size The size of the provided universes array.
 * @return The total number of the source's universes. If this is greater than universes_size, then only universes_size
 * universes were written to the universes array. If the source was not found, 0 is returned.
 */
size_t sacn_source_get_universes(sacn_source_t handle, uint16_t* universes, size_t universes_size)
{
  size_t total_num_universes = 0;

#if SACN_SOURCE_ENABLED
  if (sacn_lock())
  {
    // Look up source state
    SacnSource* source = NULL;
    if (lookup_source_state(handle, 0, &source, NULL) == kEtcPalErrOk)
    {
      // Use total number of universes as the return value
      total_num_universes = etcpal_rbtree_size(&source->universes);

      // Copy out the universes
      EtcPalRbIter tree_iter;
      etcpal_rbiter_init(&tree_iter);
      uint16_t* universe = etcpal_rbiter_first(&tree_iter, &source->universes);
      for (size_t i = 0; universe && universes && (i < universes_size); ++i)
      {
        universes[i] = *universe;
        universe = etcpal_rbiter_next(&tree_iter);
      }
    }

    sacn_unlock();
  }
#endif

  return total_num_universes;
}

/**
 * @brief Add a unicast destination for a source's universe.
 *
 * This will reset transmission suppression and include the new unicast destination in transmissions for the universe.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL.
 * @return #kEtcPalErrOk: Address added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrExists: The unicast destination was already added to this universe on this source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_SOURCE_INVALID)
      result = kEtcPalErrInvalid;
    else if (!UNIVERSE_ID_VALID(universe))
      result = kEtcPalErrInvalid;
    else if (!dest)
      result = kEtcPalErrInvalid;
    else if (ETCPAL_IP_IS_INVALID(dest))
      result = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    // Look up the state
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_source_state(handle, universe, &source_state, &universe_state);

    // Add unicast destination
    SacnUnicastDestination* unicast_dest = NULL;
    if (result == kEtcPalErrOk)
    {
      unicast_dest = add_sacn_unicast_dest(universe_state, dest);
      if (!unicast_dest)
      {
        if (lookup_unicast_dest(handle, universe, dest, NULL) == kEtcPalErrOk)
          result = kEtcPalErrExists;
        else
          result = kEtcPalErrNoMem;
      }
    }

    // Initialize & reset transmission suppression
    if (result == kEtcPalErrOk)
    {
      unicast_dest->terminating = false;
      unicast_dest->num_terminations_sent = 0;
      reset_transmission_suppression(source_state, universe_state, true, true);
    }

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Remove a unicast destination on a source's universe.
 *
 * This queues the address for removal. The removal actually occurs either on the thread or on a call to
 * sacn_source_process_manual() after an additional three packets have been sent with the "Stream_Terminated" option
 * set.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL, and must match the address passed to
 * sacn_source_add_unicast_destination().
 */
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
#if SACN_SOURCE_ENABLED
  // Validate & lock
  if (dest && sacn_lock())
  {
    // Look up unicast destination
    SacnUnicastDestination* unicast_dest = NULL;
    lookup_unicast_dest(handle, universe, dest, &unicast_dest);

    // Initiate termination
    if (unicast_dest)
      set_unicast_dest_terminating(unicast_dest);

    sacn_unlock();
  }
#endif
}

/**
 * @brief Obtain a list of a universe's unicast destinations.
 *
 * @param[in] handle Handle to the source of the universe in question.
 * @param[in] universe The universe for which to obtain the list of unicast destinations.
 * @param[out] destinations A pointer to an application-owned array where the unicast destination list will be written.
 * @param[in] destinations_size The size of the provided destinations array.
 * @return The total number of unicast destinations for the given universe. If this is greater than destinations_size,
 * then only destinations_size addresses were written to the destinations array. If the source was not found, 0 is
 * returned.
 */
size_t sacn_source_get_unicast_destinations(sacn_source_t handle, uint16_t universe, EtcPalIpAddr* destinations,
                                            size_t destinations_size)
{
  size_t total_num_dests = 0;

#if SACN_SOURCE_ENABLED
  if (sacn_lock())
  {
    // Look up universe state
    SacnSourceUniverse* universe_state = NULL;
    if (lookup_source_state(handle, universe, NULL, &universe_state) == kEtcPalErrOk)
    {
      // Use total number of destinations as the return value
      total_num_dests = etcpal_rbtree_size(&universe_state->unicast_dests);

      // Copy out the destinations
      EtcPalRbIter tree_iter;
      etcpal_rbiter_init(&tree_iter);
      EtcPalIpAddr* dest = etcpal_rbiter_first(&tree_iter, &universe_state->unicast_dests);
      for (size_t i = 0; dest && destinations && (i < destinations_size); ++i)
      {
        destinations[i] = *dest;
        dest = etcpal_rbiter_next(&tree_iter);
      }
    }

    sacn_unlock();
  }
#endif

  return total_num_dests;
}

/**
 * @brief Change the priority of a universe on a sACN source.
 *
 * This function will update the packet buffers with the new priority. If this universe is transmitting NULL start code
 * or PAP data, the logic that slows down packet transmission due to inactivity will be reset.
 *
 * @param[in] handle Handle to the source for which to set the priority.
 * @param[in] universe Universe to change.
 * @param[in] new_priority New priority of the data sent from this source. Valid range is 0 to 200,
 *                         inclusive.
 * @return #kEtcPalErrOk: Priority set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_priority(sacn_source_t handle, uint16_t universe, uint8_t new_priority)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_SOURCE_INVALID)
      result = kEtcPalErrInvalid;
    else if (!UNIVERSE_ID_VALID(universe))
      result = kEtcPalErrInvalid;
    else if (new_priority > 200)
      result = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    // Look up the source and universe state.
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_source_state(handle, universe, &source_state, &universe_state);

    // Set the priority.
    if (result == kEtcPalErrOk)
      set_universe_priority(source_state, universe_state, new_priority);

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Change the send_preview option on a universe of a sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
 *
 * This function will update the packet buffers with the new option. If this universe is transmitting NULL start code
 * or PAP data, the logic that slows down packet transmission due to inactivity will be reset.
 *
 * @param[in] handle Handle to the source for which to set the Preview_Data option.
 * @param[in] universe The universe to change.
 * @param[in] new_preview_flag The new send_preview option.
 * @return #kEtcPalErrOk: send_preview option set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_preview_flag(sacn_source_t handle, uint16_t universe, bool new_preview_flag)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_SOURCE_INVALID)
      result = kEtcPalErrInvalid;
    else if (!UNIVERSE_ID_VALID(universe))
      result = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    // Look up the source and universe state.
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_source_state(handle, universe, &source_state, &universe_state);

    // Set the preview flag.
    if (result == kEtcPalErrOk)
      set_preview_flag(source_state, universe_state, new_preview_flag);

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Changes the synchronization universe for a universe of a sACN source.
 *
 * This will change the synchronization universe used by a sACN universe on the source.
 * If this value is 0, synchronization is turned off for that universe.
 *
 * This function will update the packet buffers with the new sync universe. If this universe is transmitting NULL start
 * code or PAP data, the logic that slows down packet transmission due to inactivity will be reset.
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe The universe to change.
 * @param[in] new_sync_universe The new synchronization universe to set.
 * @return #kEtcPalErrOk: sync_universe set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_synchronization_universe(sacn_source_t handle, uint16_t universe,
                                                           uint16_t new_sync_universe)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_sync_universe);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Immediately sends the provided sACN start code & data.
 *
 * Immediately sends a sACN packet with the provided start code and data.
 * This function is intended for sACN packets that have a startcode other than 0 or 0xdd, since those
 * start codes are taken care of by either the thread or sacn_source_process_manual().
 *
 * @param[in] handle Handle to the source.
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
etcpal_error_t sacn_source_send_now(sacn_source_t handle, uint16_t universe, uint8_t start_code, const uint8_t* buffer,
                                    size_t buflen)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  etcpal_error_t result = kEtcPalErrOk;

  // Verify module initialized.
  if (!sacn_initialized())
    result = kEtcPalErrNotInit;

  // Check for invalid arguments.
  if (result == kEtcPalErrOk)
  {
    if (handle == SACN_SOURCE_INVALID)
      result = kEtcPalErrInvalid;
    else if (!UNIVERSE_ID_VALID(universe))
      result = kEtcPalErrInvalid;
    else if (buflen > DMX_ADDRESS_COUNT)
      result = kEtcPalErrInvalid;
    else if (!buffer || (buflen == 0))
      result = kEtcPalErrInvalid;
  }

  if (sacn_lock())
  {
    // Look up state
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_source_state(handle, universe, &source_state, &universe_state);

    if (result == kEtcPalErrOk)
    {
      // Initialize send buffer
      uint8_t send_buf[SACN_MTU];
      init_send_buf(send_buf, start_code, &source_state->cid, source_state->name, universe_state->priority,
                    universe_state->universe_id, universe_state->sync_universe, universe_state->send_preview);
      update_data(send_buf, buffer, (uint16_t)buflen, false);

      // Send on the network
      send_data_multicast_ipv4_ipv6(source_state, universe_state, send_buf);
      send_data_unicast_ipv4_ipv6(source_state, universe_state, send_buf);
      increment_sequence_number(universe_state);
    }

    sacn_unlock();
  }

  return result;
}

/**
 * @brief Indicate that a new synchronization packet should be sent on the given synchronization universe.
 *
 * This will cause the source to transmit a synchronization packet on the given synchronization universe.
 *
 * TODO: At this time, synchronization is not supported by this library, so this function is not implemented.
 *
 * @param[in] handle Handle to the source.
 * @param[in] sync_universe The synchronization universe to send on.
 * @return #kEtcPalErrOk: Message successfully sent.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_send_synchronization(sacn_source_t handle, uint16_t sync_universe)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(sync_universe);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Copies the universe's dmx values into the packet to be sent on the next threaded or manual update.
 *
 * This function will update the outgoing packet values, and reset the logic that slows down packet transmission due to
 * inactivity.
 *
 * When you don't have per-address priority changes to make, use this function. Otherwise, use
 * sacn_source_update_values_and_pap().
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL.
 * @param[in] new_values_size Size of new_values. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_values(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                               size_t new_values_size)
{
#if SACN_SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_state(handle, universe, &source_state, &universe_state);
    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, NULL, 0, false);
    sacn_unlock();
  }
#endif  // SACN_SOURCE_ENABLED
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
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL.
 * @param[in] new_values_size Size of new_values. This must be no larger than #DMX_ADDRESS_COUNT.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_values_and_pap(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                       size_t new_values_size, const uint8_t* new_priorities,
                                       size_t new_priorities_size)
{
#if SACN_SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && (new_priorities_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_state(handle, universe, &source_state, &universe_state);

    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, new_priorities,
                              new_priorities_size, false);

    // Stop using PAPs if new_priorities is NULL
    if (!new_priorities)
      universe_state->has_pap_data = false;

    sacn_unlock();
  }
#endif  // SACN_SOURCE_ENABLED
}

/**
 * @brief Like sacn_source_update_values(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next threaded or manual update, and will reset
 * the logic that slows down packet transmission due to inactivity. Additionally, the packet to be sent will have its
 * force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to sacn_source_update_values().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL.
 * @param[in] new_values_size Size of new_values. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_values_and_force_sync(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                              size_t new_values_size)
{
#if SACN_SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_state(handle, universe, &source_state, &universe_state);
    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, NULL, 0, true);
    sacn_unlock();
  }
#endif  // SACN_SOURCE_ENABLED
}

/**
 * @brief Like sacn_source_update_values_and_pap(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next threaded or manual update, and will reset
 * the logic that slows down packet transmission due to inactivity. Additionally, both packets to be sent by this call
 * will have their force_synchronization option flags set.
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
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL.
 * @param[in] new_values_size Size of new_values. This must be no larger than #DMX_ADDRESS_COUNT.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities. This must be no larger than #DMX_ADDRESS_COUNT.
 */
void sacn_source_update_values_and_pap_and_force_sync(sacn_source_t handle, uint16_t universe,
                                                      const uint8_t* new_values, size_t new_values_size,
                                                      const uint8_t* new_priorities, size_t new_priorities_size)
{
#if SACN_SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && (new_priorities_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SacnSource* source_state = NULL;
    SacnSourceUniverse* universe_state = NULL;
    lookup_source_state(handle, universe, &source_state, &universe_state);

    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, new_priorities,
                              new_priorities_size, true);

    // Stop using PAPs if new_priorities is NULL
    if (!new_priorities)
      universe_state->has_pap_data = false;

    sacn_unlock();
  }
#endif  // SACN_SOURCE_ENABLED
}

/**
 * @brief Trigger the transmission of sACN packets for all universes of sources that were created with
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
 * @return Current number of manual sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and actually
 *         been destroyed.
 */
int sacn_source_process_manual(void)
{
#if !SACN_SOURCE_ENABLED
  return 0;
#endif

  return process_internal(true);
}

/**
 * @brief Resets the underlying network sockets for a universe.
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the universe is considered to be updated and have new values and priorities.
 * It's as if the source just started sending values on that universe.
 *
 * If this call fails, the caller must call sacn_source_destroy(), because the source may be in an
 * invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the source for which to reset the networking.
 * @param[in] universe Universe to reset netowrk interfaces for.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Source changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or the universe was not found on this
 *                              source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_reset_networking(sacn_source_t handle, uint16_t universe, SacnMcastInterface* netints,
                                            size_t num_netints)
{
#if !SACN_SOURCE_ENABLED
  return kEtcPalErrNotInit;
#endif

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);

  return kEtcPalErrNotImpl;
}

/**
 * @brief Obtain a list of a universe's network interfaces.
 *
 * @param[in] handle Handle to the source that includes the universe.
 * @param[in] universe The universe for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the universe. If this is greater than netints_size, then only
 * netints_size addresses were written to the netints array. If the source or universe were not found, 0 is returned.
 */
size_t sacn_source_get_network_interfaces(sacn_source_t handle, uint16_t universe, EtcPalMcastNetintId* netints,
                                          size_t netints_size)
{
  size_t total_num_network_interfaces = 0;

#if SACN_SOURCE_ENABLED
  if (sacn_lock())
  {
    // Look up universe state
    SacnSourceUniverse* universe_state = NULL;
    if (lookup_source_state(handle, universe, NULL, &universe_state) == kEtcPalErrOk)
    {
      total_num_network_interfaces = universe_state->num_netints;

      // Copy out the netints
      for (size_t i = 0; netints && (i < netints_size) && (i < total_num_network_interfaces); ++i)
        netints[i] = universe_state->netints[i];
    }

    sacn_unlock();
  }
#endif

  return total_num_network_interfaces;
}

bool source_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  return (handle_val == SACN_SOURCE_INVALID) || (lookup_source_state(handle_val, 0, NULL, NULL) == kEtcPalErrOk);
}

// Needs lock
etcpal_error_t start_tick_thread()
{
  shutting_down = false;
  EtcPalThreadParams params = ETCPAL_THREAD_PARAMS_INIT;

  if (etcpal_thread_create(&source_thread_handle, &params, source_thread_function, NULL) != kEtcPalErrOk)
    return kEtcPalErrSys;

  return kEtcPalErrOk;
}

// Takes lock
void stop_tick_thread()
{
  etcpal_thread_t thread_handle;

  if (sacn_lock())
  {
    shutting_down = true;  // Trigger thread-based sources to terminate
    thread_handle = source_thread_handle;
    sacn_unlock();
  }

  // Wait for thread-based sources to terminate (assuming application already cleaned up manual sources)
  etcpal_thread_join(&thread_handle);
}

// Takes lock
void source_thread_function(void* arg)
{
  ETCPAL_UNUSED_ARG(arg);

  bool keep_running_thread = true;
  int num_thread_based_sources = 0;

  EtcPalTimer interval_timer;
  etcpal_timer_start(&interval_timer, SOURCE_THREAD_INTERVAL);

  while (keep_running_thread || (num_thread_based_sources > 0))
  {
    num_thread_based_sources = process_internal(false);

    etcpal_thread_sleep(etcpal_timer_remaining(&interval_timer));
    etcpal_timer_reset(&interval_timer);

    if (sacn_lock())
    {
      keep_running_thread = !shutting_down;
      sacn_unlock();
    }
  }
}

// Takes lock
int process_internal(bool process_manual)
{
  int num_sources_tracked = 0;

  if (sacn_lock())
  {
    num_sources_tracked = process_sources(process_manual);
    sacn_unlock();
  }

  return num_sources_tracked;
}

// Needs lock
int process_sources(bool process_manual)
{
  int num_sources_tracked = 0;

  // For each source
  EtcPalRbIter source_iter;
  etcpal_rbiter_init(&source_iter);
  SacnSource* source = etcpal_rbiter_first(&source_iter, get_sacn_sources());
  while (source)
  {
    bool source_incremented = false;

    // If this is the kind of source we want to process (manual vs. thread-based)
    if (source->process_manually == process_manual)
    {
      // If the Source API is shutting down, cause this source to terminate (if thread-based)
      if (!process_manual && shutting_down)
        set_source_terminating(source);

      // Count the sources of the kind being processed by this function
      ++num_sources_tracked;

      // Universe processing
      process_universe_discovery(source);
      process_universes(source);

      // Clean up this source if needed
      if (source->terminating && (etcpal_rbtree_size(&source->universes) == 0))
      {
        remove_sacn_source(&source, &source_iter);
        source_incremented = true;
      }
    }

    if (!source_incremented)
      source = etcpal_rbiter_next(&source_iter);
  }

  return num_sources_tracked;
}

// Needs lock
void process_universe_discovery(SacnSource* source)
{
  // Send another universe discovery packet if it's time
  if (!source->terminating && etcpal_timer_is_expired(&source->universe_discovery_timer))
  {
    send_universe_discovery(source);
    etcpal_timer_reset(&source->universe_discovery_timer);
  }
}

// Needs lock
void process_universes(SacnSource* source)
{
  // For each universe of this source
  EtcPalRbIter universe_iter;
  etcpal_rbiter_init(&universe_iter);
  SacnSourceUniverse* universe = etcpal_rbiter_first(&universe_iter, &source->universes);
  while (universe)
  {
    bool universe_incremented = false;

    // Unicast destination-specific processing
    process_unicast_dests(source, universe);

    // Either transmit start codes 0x00 & 0xDD, or terminate and clean up universe
    if (universe->terminating)
      process_universe_termination(source, universe, &universe_iter);
    else
      process_universe_null_pap_transmission(source, universe);

    if (!universe_incremented)
      universe = etcpal_rbiter_next(&universe_iter);
  }
}

// Needs lock
void process_unicast_dests(SacnSource* source, SacnSourceUniverse* universe)
{
  // For each unicast destination of this universe
  EtcPalRbIter unicast_iter;
  etcpal_rbiter_init(&unicast_iter);
  SacnUnicastDestination* dest = etcpal_rbiter_first(&unicast_iter, &universe->unicast_dests);
  while (dest)
  {
    bool dest_incremented = false;

    // Terminate and clean up this unicast destination if needed
    if (dest->terminating)
    {
      if ((dest->num_terminations_sent < 3) && universe->has_null_data)
        send_termination_unicast(source, universe, dest);

      if ((dest->num_terminations_sent >= 3) || !universe->has_null_data)
      {
        remove_sacn_unicast_dest(universe, &dest, &unicast_iter);
        dest_incremented = true;
      }
    }

    if (!dest_incremented)
      dest = etcpal_rbiter_next(&unicast_iter);
  }
}

// Needs lock
bool process_universe_termination(SacnSource* source, SacnSourceUniverse* universe, EtcPalRbIter* universe_iter)
{
  bool universe_incremented = false;

  if ((universe->num_terminations_sent < 3) && universe->has_null_data)
    send_termination_multicast(source, universe);

  if (((universe->num_terminations_sent >= 3) && (etcpal_rbtree_size(&universe->unicast_dests) == 0)) ||
      !universe->has_null_data)
  {
    // Update num_active_universes if needed
    if (IS_PART_OF_UNIVERSE_DISCOVERY(universe))
      --source->num_active_universes;

    // Update the netints tree
    for (size_t i = 0; i < universe->num_netints; ++i)
    {
      SacnSourceNetint* netint_state = lookup_source_netint(source, &universe->netints[i]);

      if (netint_state)
      {
        if (netint_state->num_refs > 0)
          --netint_state->num_refs;

        if (netint_state->num_refs == 0)
          remove_sacn_source_netint(source, &netint_state);
      }
    }

    remove_sacn_source_universe(source, &universe, universe_iter);
    universe_incremented = true;
  }

  return universe_incremented;
}

// Needs lock
void process_universe_null_pap_transmission(SacnSource* source, SacnSourceUniverse* universe)
{
  // If 0x00 data is ready to send
  if (universe->has_null_data && ((universe->null_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS) ||
                                  etcpal_timer_is_expired(&universe->null_keep_alive_timer)))
  {
    // Send 0x00 data & reset the keep-alive timer
    send_null_data_multicast(source, universe);
    send_null_data_unicast(source, universe);
    process_null_sent(universe);
    etcpal_timer_reset(&universe->null_keep_alive_timer);
  }
#if SACN_ETC_PRIORITY_EXTENSION
  // If 0xDD data is ready to send
  if (universe->has_pap_data && ((universe->pap_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS) ||
                                 etcpal_timer_is_expired(&universe->pap_keep_alive_timer)))
  {
    // Send 0xDD data & reset the keep-alive timer
    send_pap_data_multicast(source, universe);
    send_pap_data_unicast(source, universe);
    process_pap_sent(universe);
    etcpal_timer_reset(&universe->pap_keep_alive_timer);
  }
#endif
}

// Needs lock
void send_data_multicast_ipv4_ipv6(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf)
{
  // Send multicast on IPv4 and/or IPv6
  if (!universe->send_unicast_only)
  {
    if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
      sacn_send_multicast(universe->universe_id, kEtcPalIpTypeV4, send_buf, universe->netints, universe->num_netints);
    if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
      sacn_send_multicast(universe->universe_id, kEtcPalIpTypeV6, send_buf, universe->netints, universe->num_netints);
  }
}

// Needs lock
void send_data_unicast_ipv4_ipv6(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf)
{
  // Send unicast on IPv4 and/or IPv6
  if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
    send_data_unicast(kEtcPalIpTypeV4, send_buf, &universe->unicast_dests);
  if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
    send_data_unicast(kEtcPalIpTypeV6, send_buf, &universe->unicast_dests);
}

// Needs lock
void increment_sequence_number(SacnSourceUniverse* universe)
{
  ++universe->seq_num;
  universe->null_send_buf[SACN_SEQ_OFFSET] = universe->seq_num;
#if SACN_ETC_PRIORITY_EXTENSION
  universe->pap_send_buf[SACN_SEQ_OFFSET] = universe->seq_num;
#endif
}

// Needs lock
void send_null_data_multicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  send_data_multicast_ipv4_ipv6(source, universe, universe->null_send_buf);
}

// Needs lock
void send_null_data_unicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  send_data_unicast_ipv4_ipv6(source, universe, universe->null_send_buf);
}

// Needs lock
void process_null_sent(SacnSourceUniverse* universe)
{
  increment_sequence_number(universe);

  if (universe->null_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS)
    ++universe->null_packets_sent_before_suppression;
}

#if SACN_ETC_PRIORITY_EXTENSION
// Needs lock
void send_pap_data_multicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  send_data_multicast_ipv4_ipv6(source, universe, universe->pap_send_buf);
}

// Needs lock
void send_pap_data_unicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  send_data_unicast_ipv4_ipv6(source, universe, universe->pap_send_buf);
}

// Needs lock
void process_pap_sent(SacnSourceUniverse* universe)
{
  increment_sequence_number(universe);

  if (universe->pap_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS)
    ++universe->pap_packets_sent_before_suppression;
}
#endif

// Needs lock
void send_termination_multicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  // Repurpose null_send_buf for the termination packet
  bool old_terminated_opt = TERMINATED_OPT_SET(universe->null_send_buf);
  SET_TERMINATED_OPT(universe->null_send_buf, true);

  // Send the termination packet on multicast only
  send_null_data_multicast(source, universe);
  process_null_sent(universe);

  // Increment the termination counter
  ++universe->num_terminations_sent;

  // Revert terminated flag
  SET_TERMINATED_OPT(universe->null_send_buf, old_terminated_opt);
}

// Needs lock
void send_termination_unicast(const SacnSource* source, SacnSourceUniverse* universe, SacnUnicastDestination* dest)
{
  // Repurpose null_send_buf for the termination packet
  bool old_terminated_opt = TERMINATED_OPT_SET(universe->null_send_buf);
  SET_TERMINATED_OPT(universe->null_send_buf, true);

  // Send the termination packet on unicast only
  if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
    send_data_to_single_unicast_dest(kEtcPalIpTypeV4, universe->null_send_buf, dest);
  if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
    send_data_to_single_unicast_dest(kEtcPalIpTypeV6, universe->null_send_buf, dest);

  process_null_sent(universe);

  // Increment the termination counter
  ++dest->num_terminations_sent;

  // Revert terminated flag
  SET_TERMINATED_OPT(universe->null_send_buf, old_terminated_opt);
}

// Needs lock
void send_universe_discovery(SacnSource* source)
{
  // Determine the set of network interfaces used by all universes of this source
#if SACN_DYNAMIC_MEM
  size_t netints_size = etcpal_rbtree_size(&source->netints);
  EtcPalMcastNetintId* netints = netints_size ? calloc(netints_size, sizeof(EtcPalMcastNetintId)) : NULL;
#else
  EtcPalMcastNetintId netints[SACN_MAX_NETINTS];
  size_t netints_size = SACN_MAX_NETINTS;
#endif

  size_t num_netints = 0;
  EtcPalRbIter netint_iter;
  etcpal_rbiter_init(&netint_iter);
  EtcPalMcastNetintId* netint = etcpal_rbiter_first(&netint_iter, &source->netints);
  while (netints && netint && (num_netints < netints_size))
  {
    netints[num_netints] = *netint;
    ++num_netints;
    netint = etcpal_rbiter_next(&netint_iter);
  }

  // If there are network interfaces to send on
  if (netints && (num_netints > 0))
  {
    // Initialize universe iterator and page number
    EtcPalRbIter universe_iter;
    etcpal_rbiter_init(&universe_iter);
    uint8_t page_number = 0u;

    // Pack the next page & loop while there's a page to send
    while (pack_universe_discovery_page(source, &universe_iter, page_number) > 0)
    {
      // Send multicast on IPv4 and/or IPv6
      if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
      {
        sacn_send_multicast(SACN_DISCOVERY_UNIVERSE, kEtcPalIpTypeV4, source->universe_discovery_send_buf, netints,
                            num_netints);
      }

      if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
      {
        sacn_send_multicast(SACN_DISCOVERY_UNIVERSE, kEtcPalIpTypeV6, source->universe_discovery_send_buf, netints,
                            num_netints);
      }

      // Increment sequence number & page number
      ++source->universe_discovery_send_buf[SACN_SEQ_OFFSET];
      ++page_number;
    }
  }

#if SACN_DYNAMIC_MEM
  if (netints)
    free(netints);
#endif
}

// Needs lock
void send_data_unicast(etcpal_iptype_t ip_type, const uint8_t* send_buf, EtcPalRbTree* dests)
{
  if (dests)
  {
    EtcPalRbIter tree_iter;
    etcpal_rbiter_init(&tree_iter);

    // For each unicast destination, send the data
    for (SacnUnicastDestination* dest = etcpal_rbiter_first(&tree_iter, dests); dest;
         dest = etcpal_rbiter_next(&tree_iter))
      send_data_to_single_unicast_dest(ip_type, send_buf, dest);
  }
}

// Needs lock
void send_data_to_single_unicast_dest(etcpal_iptype_t ip_type, const uint8_t* send_buf,
                                      const SacnUnicastDestination* dest)
{
  // If this destination matches the IP type, then send to it.
  if (dest && (dest->dest_addr.type == ip_type))
    sacn_send_unicast(send_buf, &dest->dest_addr);
}

// Needs lock
int pack_universe_discovery_page(SacnSource* source, EtcPalRbIter* universe_iter, uint8_t page_number)
{
  // Initialize packing pointer and universe counter
  uint8_t* pcur = &source->universe_discovery_send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE];
  int num_universes_packed = 0;

  // Iterate up to 512 universes (sorted)
  for (const SacnSourceUniverse* universe = (page_number == 0) ? etcpal_rbiter_first(universe_iter, &source->universes)
                                                               : etcpal_rbiter_next(universe_iter);
       universe && (num_universes_packed < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE);
       universe = etcpal_rbiter_next(universe_iter))
  {
    // If this universe has NULL start code data at a bare minimum & is not unicast-only
    if (IS_PART_OF_UNIVERSE_DISCOVERY(universe))
    {
      // Pack the universe ID
      etcpal_pack_u16b(pcur, universe->universe_id);
      pcur += 2;

      // Increment number of universes packed
      ++num_universes_packed;
    }
  }

  // Update universe count, page, and last page PDU fields
  SET_UNIVERSE_COUNT(source->universe_discovery_send_buf, num_universes_packed);
  SET_PAGE(source->universe_discovery_send_buf, page_number);
  SET_LAST_PAGE(source->universe_discovery_send_buf,
                (uint8_t)(source->num_active_universes / SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE));

  // Return number of universes packed
  return num_universes_packed;
}

// Needs lock
void init_send_buf(uint8_t* send_buf, uint8_t start_code, const EtcPalUuid* source_cid, const char* source_name,
                   uint8_t priority, uint16_t universe, uint16_t sync_universe, bool send_preview)
{
  memset(send_buf, 0, SACN_MTU);
  size_t written = 0;
  written += pack_sacn_root_layer(send_buf, SACN_DATA_HEADER_SIZE, false, source_cid);
  written += pack_sacn_data_framing_layer(&send_buf[written], 0, VECTOR_E131_DATA_PACKET, source_name, priority,
                                          sync_universe, 0, send_preview, false, false, universe);
  written += pack_sacn_dmp_layer_header(&send_buf[written], start_code, 0);
}

// Needs lock
void update_data(uint8_t* send_buf, const uint8_t* new_data, uint16_t new_data_size, bool force_sync)
{
  ETCPAL_UNUSED_ARG(force_sync);  // TODO sacn_sync

  // Set force sync flag
  SET_FORCE_SYNC_OPT(send_buf, force_sync);

  // Update the size/count fields for the new data size (slot count)
  SET_DATA_SLOT_COUNT(send_buf, new_data_size);

  // Copy data into the send buffer immediately after the start code
  memcpy(&send_buf[SACN_DATA_HEADER_SIZE], new_data, new_data_size);
}

// Needs lock
void update_levels(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_levels,
                   size_t new_levels_size, bool force_sync)
{
  bool was_part_of_discovery = IS_PART_OF_UNIVERSE_DISCOVERY(universe_state);

  update_data(universe_state->null_send_buf, new_levels, (uint16_t)new_levels_size, force_sync);
  universe_state->has_null_data = true;
  reset_transmission_suppression(source_state, universe_state, true, false);

  if (!was_part_of_discovery && IS_PART_OF_UNIVERSE_DISCOVERY(universe_state))
    ++source_state->num_active_universes;
}

#if SACN_ETC_PRIORITY_EXTENSION
// Needs lock
void update_paps(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_priorities,
                 size_t new_priorities_size, bool force_sync)
{
  update_data(universe_state->pap_send_buf, new_priorities, (uint16_t)new_priorities_size, force_sync);
  universe_state->has_pap_data = true;
  reset_transmission_suppression(source_state, universe_state, false, true);
}
#endif

// Needs lock
void update_levels_and_or_paps(SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels,
                               size_t new_levels_size, const uint8_t* new_priorities, size_t new_priorities_size,
                               bool force_sync)
{
  if (source && universe)
  {
    // Update 0x00 values
    if (new_levels)
      update_levels(source, universe, new_levels, new_levels_size, force_sync);
#if SACN_ETC_PRIORITY_EXTENSION
    // Update 0xDD values
    if (new_priorities)
      update_paps(source, universe, new_priorities, new_priorities_size, force_sync);
#endif
  }
}

// Needs lock
void set_source_terminating(SacnSource* source)
{
  // If the source isn't already terminating
  if (source && !source->terminating)
  {
    // Set the source's terminating flag
    source->terminating = true;

    // Set terminating for each universe of this source
    EtcPalRbIter tree_iter;
    etcpal_rbiter_init(&tree_iter);

    for (SacnSourceUniverse* universe = etcpal_rbiter_first(&tree_iter, &source->universes); universe;
         universe = etcpal_rbiter_next(&tree_iter))
    {
      set_universe_terminating(universe);
    }
  }
}

// Needs lock
void set_universe_terminating(SacnSourceUniverse* universe)
{
  // If the universe isn't already terminating
  if (universe && !universe->terminating)
  {
    // Set the universe's terminating flag and termination counter
    universe->terminating = true;
    universe->num_terminations_sent = 0;

    // Set terminating for each unicast destination of this universe
    EtcPalRbIter tree_iter;
    etcpal_rbiter_init(&tree_iter);

    for (SacnUnicastDestination* dest = etcpal_rbiter_first(&tree_iter, &universe->unicast_dests); dest;
         dest = etcpal_rbiter_next(&tree_iter))
    {
      set_unicast_dest_terminating(dest);
    }
  }
}

// Needs lock
void set_unicast_dest_terminating(SacnUnicastDestination* dest)
{
  // If the unicast destination isn't already terminating
  if (dest && !dest->terminating)
  {
    // Set the unicast destination's terminating flag and termination counter
    dest->terminating = true;
    dest->num_terminations_sent = 0;
  }
}

// Needs lock
void reset_transmission_suppression(const SacnSource* source, SacnSourceUniverse* universe, bool reset_null,
                                    bool reset_pap)
{
  if (reset_null)
  {
    universe->null_packets_sent_before_suppression = 0;

    if (universe->has_null_data)
      etcpal_timer_start(&universe->null_keep_alive_timer, source->keep_alive_interval);
  }

  if (reset_pap)
  {
    universe->pap_packets_sent_before_suppression = 0;

    if (universe->has_pap_data)
      etcpal_timer_start(&universe->pap_keep_alive_timer, source->keep_alive_interval);
  }
}

// Needs lock
void set_source_name(SacnSource* source, const char* new_name)
{
  // Update the name in the source state and universe discovery buffer
  strncpy(source->name, new_name, SACN_SOURCE_NAME_MAX_LEN);
  strncpy((char*)(&source->universe_discovery_send_buf[SACN_SOURCE_NAME_OFFSET]), new_name, SACN_SOURCE_NAME_MAX_LEN);

  // For each universe:
  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);
  for (SacnSourceUniverse* universe = etcpal_rbiter_first(&tree_iter, &source->universes); universe;
       universe = etcpal_rbiter_next(&tree_iter))

  {
    // Update the source name in this universe's send buffers
    strncpy((char*)(&universe->null_send_buf[SACN_SOURCE_NAME_OFFSET]), new_name, SACN_SOURCE_NAME_MAX_LEN);
    strncpy((char*)(&universe->pap_send_buf[SACN_SOURCE_NAME_OFFSET]), new_name, SACN_SOURCE_NAME_MAX_LEN);

    // Reset transmission suppression for start codes 0x00 and 0xDD
    reset_transmission_suppression(source, universe, true, true);
  }
}

// Needs lock
void set_universe_priority(const SacnSource* source, SacnSourceUniverse* universe, uint8_t priority)
{
  universe->priority = priority;
  universe->null_send_buf[SACN_PRI_OFFSET] = priority;
  universe->pap_send_buf[SACN_PRI_OFFSET] = priority;
  reset_transmission_suppression(source, universe, true, true);
}

// Needs lock
void set_preview_flag(const SacnSource* source, SacnSourceUniverse* universe, bool preview)
{
  universe->send_preview = preview;
  SET_PREVIEW_OPT(universe->null_send_buf, preview);
  SET_PREVIEW_OPT(universe->pap_send_buf, preview);
  reset_transmission_suppression(source, universe, true, true);
}
