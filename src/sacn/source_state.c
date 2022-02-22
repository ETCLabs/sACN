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

#include "sacn/private/common.h"

#include "sacn/private/source_loss.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"
#include "sacn/private/sockets.h"
#include "sacn/private/source_state.h"
#include "sacn/private/util.h"
#include "etcpal/handle_manager.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "etcpal/rbtree.h"
#include "etcpal/timer.h"

#if SACN_SOURCE_ENABLED

// Suppress strncpy() warning on Windows/MSVC.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/****************************** Private macros *******************************/

#define SOURCE_THREAD_INTERVAL 23
#define NUM_PRE_SUPPRESSION_PACKETS 4
#define IS_PART_OF_UNIVERSE_DISCOVERY(universe) (universe->has_level_data && !universe->send_unicast_only)

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

static int process_sources(process_sources_behavior_t behavior);
static void process_universe_discovery(SacnSource* source);
static void process_universes(SacnSource* source);
static void process_unicast_dests(SacnSource* source, SacnSourceUniverse* universe, bool* terminating);
static void process_universe_termination(SacnSource* source, size_t index, bool unicast_terminating);
static void transmit_levels_and_pap_when_needed(SacnSource* source, SacnSourceUniverse* universe);
static void send_termination_multicast(const SacnSource* source, SacnSourceUniverse* universe);
static void send_termination_unicast(const SacnSource* source, SacnSourceUniverse* universe,
                                     SacnUnicastDestination* dest);
static void send_universe_discovery(SacnSource* source);
static int pack_universe_discovery_page(SacnSource* source, size_t* universe_index, uint8_t page_number);
static void update_levels(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_levels,
                          size_t new_levels_size, force_sync_behavior_t force_sync);
#if SACN_ETC_PRIORITY_EXTENSION
static void update_pap(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_priorities,
                       size_t new_priorities_size, force_sync_behavior_t force_sync);
static void zero_levels_where_pap_is_zero(SacnSourceUniverse* universe_state);
#endif
static void remove_from_source_netints(SacnSource* source, const EtcPalMcastNetintId* id);
static void reset_unicast_dest(SacnUnicastDestination* dest);
static void reset_universe(SacnSourceUniverse* universe);
static void cancel_termination_if_not_removing(SacnSourceUniverse* universe);

/*************************** Function definitions ****************************/

etcpal_error_t sacn_source_state_init(void)
{
  shutting_down = false;
  init_int_handle_manager(&source_handle_mgr, -1, source_handle_in_use, NULL);

  return kEtcPalErrOk;
}

void sacn_source_state_deinit(void)
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

bool source_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  SacnSource* tmp = NULL;
  return (handle_val == SACN_SOURCE_INVALID) || (lookup_source(handle_val, &tmp) == kEtcPalErrOk);
}

// Needs lock
etcpal_error_t start_tick_thread()
{
  shutting_down = false;
  EtcPalThreadParams params = {SACN_SOURCE_THREAD_PRIORITY, SACN_SOURCE_THREAD_STACK, SACN_SOURCE_THREAD_NAME, NULL};

  return etcpal_thread_create(&source_thread_handle, &params, source_thread_function, NULL);
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

  // This thread will keep running as long as sACN is initialized (while keep_running_thread is true). On
  // deinitialization, the thread keeps running until there are no more thread-based sources (while
  // num_thread_based_sources > 0).
  while (keep_running_thread || (num_thread_based_sources > 0))
  {
    num_thread_based_sources = take_lock_and_process_sources(kProcessThreadedSources);

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
int take_lock_and_process_sources(process_sources_behavior_t behavior)
{
  int num_sources_tracked = 0;

  if (sacn_lock())
  {
    num_sources_tracked = process_sources(behavior);
    sacn_unlock();
  }

  return num_sources_tracked;
}

// Needs lock
etcpal_error_t initialize_source_thread()
{
  etcpal_error_t result = kEtcPalErrOk;

  if (!thread_initialized)
  {
    result = start_tick_thread();

    if (result == kEtcPalErrOk)
      thread_initialized = true;
  }

  return result;
}

// Needs lock
sacn_source_t get_next_source_handle()
{
  return (sacn_source_t)get_next_int_handle(&source_handle_mgr);
}

// Needs lock
int process_sources(process_sources_behavior_t behavior)
{
  int num_sources_tracked = 0;

  size_t initial_num_sources = get_num_sources();  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_sources; ++i)
  {
    SacnSource* source = get_source(initial_num_sources - 1 - i);

    // If this is the kind of source we want to process (manual vs. thread-based)
    bool process_manual = (behavior == kProcessManualSources);
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
      if (source->terminating && (source->num_universes == 0))
        remove_sacn_source(initial_num_sources - 1 - i);
    }
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
  size_t initial_num_universes = source->num_universes;  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_universes; ++i)
  {
    SacnSourceUniverse* universe = &source->universes[initial_num_universes - 1 - i];

    // Unicast destination-specific processing
    bool unicast_terminating;
    process_unicast_dests(source, universe, &unicast_terminating);

    // Either transmit start codes 0x00 & 0xDD, or terminate and clean up universe
    if (universe->termination_state == kNotTerminating)
      transmit_levels_and_pap_when_needed(source, universe);
    else
      process_universe_termination(source, initial_num_universes - 1 - i, unicast_terminating);
  }
}

// Needs lock
void process_unicast_dests(SacnSource* source, SacnSourceUniverse* universe, bool* terminating)
{
  *terminating = false;

  size_t initial_num_unicast_dests = universe->num_unicast_dests;  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_unicast_dests; ++i)
  {
    SacnUnicastDestination* dest = &universe->unicast_dests[initial_num_unicast_dests - 1 - i];

    // Terminate and clean up this unicast destination if needed
    if (dest->termination_state != kNotTerminating)
    {
      if ((dest->num_terminations_sent < 3) && universe->has_level_data)
        send_termination_unicast(source, universe, dest);

      if ((dest->num_terminations_sent >= 3) || !universe->has_level_data)
        finish_unicast_dest_termination(universe, initial_num_unicast_dests - 1 - i);
      else
        *terminating = true;
    }
  }
}

// Needs lock
void process_universe_termination(SacnSource* source, size_t index, bool unicast_terminating)
{
  SacnSourceUniverse* universe = &source->universes[index];

  if ((universe->num_terminations_sent < 3) && universe->has_level_data)
    send_termination_multicast(source, universe);

  if (((universe->num_terminations_sent >= 3) && !unicast_terminating) || !universe->has_level_data)
    finish_source_universe_termination(source, index);
}

// Needs lock
void transmit_levels_and_pap_when_needed(SacnSource* source, SacnSourceUniverse* universe)
{
  // If 0x00 data is ready to send
  if (universe->has_level_data && ((universe->level_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS) ||
                                   etcpal_timer_is_expired(&universe->level_keep_alive_timer)))
  {
    // Send 0x00 data & reset the keep-alive timer
    send_universe_multicast(source, universe, universe->level_send_buf);
    send_universe_unicast(source, universe, universe->level_send_buf);
    increment_sequence_number(universe);

    if (universe->level_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS)
      ++universe->level_packets_sent_before_suppression;

    etcpal_timer_reset(&universe->level_keep_alive_timer);
  }
#if SACN_ETC_PRIORITY_EXTENSION
  // If 0xDD data is ready to send
  if (universe->has_pap_data && ((universe->pap_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS) ||
                                 etcpal_timer_is_expired(&universe->pap_keep_alive_timer)))
  {
    // Send 0xDD data & reset the keep-alive timer
    send_universe_multicast(source, universe, universe->pap_send_buf);
    send_universe_unicast(source, universe, universe->pap_send_buf);
    increment_sequence_number(universe);

    if (universe->pap_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS)
      ++universe->pap_packets_sent_before_suppression;

    etcpal_timer_reset(&universe->pap_keep_alive_timer);
  }
#endif
}

// Needs lock
void increment_sequence_number(SacnSourceUniverse* universe)
{
  ++universe->seq_num;
  universe->level_send_buf[SACN_SEQ_OFFSET] = universe->seq_num;
#if SACN_ETC_PRIORITY_EXTENSION
  universe->pap_send_buf[SACN_SEQ_OFFSET] = universe->seq_num;
#endif
}

// Needs lock
void send_termination_multicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  // Repurpose level_send_buf for the termination packet
  bool old_terminated_opt = TERMINATED_OPT_SET(universe->level_send_buf);
  SET_TERMINATED_OPT(universe->level_send_buf, true);

  // Send the termination packet on multicast only
  send_universe_multicast(source, universe, universe->level_send_buf);
  increment_sequence_number(universe);

  // Increment the termination counter
  ++universe->num_terminations_sent;

  // Revert terminated flag
  SET_TERMINATED_OPT(universe->level_send_buf, old_terminated_opt);
}

// Needs lock
void send_termination_unicast(const SacnSource* source, SacnSourceUniverse* universe, SacnUnicastDestination* dest)
{
  // Repurpose level_send_buf for the termination packet
  bool old_terminated_opt = TERMINATED_OPT_SET(universe->level_send_buf);
  SET_TERMINATED_OPT(universe->level_send_buf, true);

  // Send the termination packet on unicast only
  sacn_send_unicast(source->ip_supported, universe->level_send_buf, &dest->dest_addr);
  increment_sequence_number(universe);

  // Increment the termination counter
  ++dest->num_terminations_sent;

  // Revert terminated flag
  SET_TERMINATED_OPT(universe->level_send_buf, old_terminated_opt);
}

// Needs lock
void send_universe_discovery(SacnSource* source)
{
  // If there are network interfaces to send on
  if (source->num_netints > 0)
  {
    // Initialize universe index and page number
    size_t universe_index = 0;
    uint8_t page_number = 0;

    // Pack the next page & loop while there's a page to send
    while (pack_universe_discovery_page(source, &universe_index, page_number) > 0)
    {
      // Send multicast on IPv4 and/or IPv6
      for (size_t i = 0; i < source->num_netints; ++i)
      {
        sacn_send_multicast(SACN_DISCOVERY_UNIVERSE, source->ip_supported, source->universe_discovery_send_buf,
                            &source->netints[i].id);
      }

      // Increment sequence number & page number
      ++source->universe_discovery_send_buf[SACN_SEQ_OFFSET];
      ++page_number;
    }
  }
}

// Needs lock
void send_universe_multicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf)
{
  if (!universe->send_unicast_only)
  {
    for (size_t i = 0; i < universe->netints.num_netints; ++i)
      sacn_send_multicast(universe->universe_id, source->ip_supported, send_buf, &universe->netints.netints[i]);
  }
}

// Needs lock
void send_universe_unicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf)
{
  for (size_t i = 0; i < universe->num_unicast_dests; ++i)
  {
    if (universe->unicast_dests[i].termination_state != kTerminatingAndRemoving)
      sacn_send_unicast(source->ip_supported, send_buf, &universe->unicast_dests[i].dest_addr);
  }
}

// Needs lock
int pack_universe_discovery_page(SacnSource* source, size_t* universe_index, uint8_t page_number)
{
  // Initialize packing pointer and universe counter
  uint8_t* pcur = &source->universe_discovery_send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE];
  int num_universes_packed = 0;

  // Iterate up to 512 universes (sorted)
  while ((*universe_index < source->num_universes) &&
         (num_universes_packed < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE))
  {
    const SacnSourceUniverse* universe = &source->universes[*universe_index];

    // If this universe has level data at a bare minimum & is not unicast-only
    if (IS_PART_OF_UNIVERSE_DISCOVERY(universe))
    {
      // Pack the universe ID
      etcpal_pack_u16b(pcur, universe->universe_id);
      pcur += 2;

      // Increment number of universes packed
      ++num_universes_packed;
    }

    ++(*universe_index);
  }

  // Update universe count, page, and last page PDU fields
  SET_UNIVERSE_COUNT(source->universe_discovery_send_buf, num_universes_packed);
  SET_PAGE(source->universe_discovery_send_buf, page_number);
  if (source->num_active_universes > 0)
  {
    SET_LAST_PAGE(source->universe_discovery_send_buf,
                  (uint8_t)((source->num_active_universes - 1) / SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE));
  }
  else
  {
    SET_LAST_PAGE(source->universe_discovery_send_buf, 0);
  }

  // Return number of universes packed
  return num_universes_packed;
}

// Needs lock
void update_levels(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_levels,
                   size_t new_levels_size, force_sync_behavior_t force_sync)
{
  bool was_part_of_discovery = IS_PART_OF_UNIVERSE_DISCOVERY(universe_state);

  cancel_termination_if_not_removing(universe_state);

  update_send_buf_data(universe_state->level_send_buf, new_levels, (uint16_t)new_levels_size, force_sync);
  universe_state->has_level_data = true;
#if SACN_ETC_PRIORITY_EXTENSION
  if (universe_state->has_pap_data)
    zero_levels_where_pap_is_zero(universe_state);  // PAP must already be updated!
#endif

  reset_transmission_suppression(source_state, universe_state, kResetLevel);

  if (!was_part_of_discovery && IS_PART_OF_UNIVERSE_DISCOVERY(universe_state))
    ++source_state->num_active_universes;
}

#if SACN_ETC_PRIORITY_EXTENSION
// Needs lock
void update_pap(SacnSource* source_state, SacnSourceUniverse* universe_state, const uint8_t* new_priorities,
                size_t new_priorities_size, force_sync_behavior_t force_sync)
{
  update_send_buf_data(universe_state->pap_send_buf, new_priorities, (uint16_t)new_priorities_size, force_sync);
  universe_state->has_pap_data = true;
  reset_transmission_suppression(source_state, universe_state, kResetPap);
}

// Needs lock
void zero_levels_where_pap_is_zero(SacnSourceUniverse* universe_state)
{
  uint16_t level_count = etcpal_unpack_u16b(&universe_state->level_send_buf[SACN_PROPERTY_VALUE_COUNT_OFFSET]) - 1;
  uint16_t pap_count = etcpal_unpack_u16b(&universe_state->pap_send_buf[SACN_PROPERTY_VALUE_COUNT_OFFSET]) - 1;
  for (uint16_t i = 0; i < level_count; ++i)
  {
    if ((i >= pap_count) || (universe_state->pap_send_buf[SACN_DATA_HEADER_SIZE + i] == 0))
      universe_state->level_send_buf[SACN_DATA_HEADER_SIZE + i] = 0;
  }
}
#endif

// Needs lock
void update_levels_and_or_pap(SacnSource* source, SacnSourceUniverse* universe, const uint8_t* new_levels,
                              size_t new_levels_size, const uint8_t* new_priorities, size_t new_priorities_size,
                              force_sync_behavior_t force_sync)
{
  if (source && universe)
  {
#if SACN_ETC_PRIORITY_EXTENSION
    // Make sure PAP is updated before levels.
    if (new_priorities)
      update_pap(source, universe, new_priorities, new_priorities_size, force_sync);
#endif
    if (new_levels)
      update_levels(source, universe, new_levels, new_levels_size, force_sync);
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

    // Set terminating for the removal of each universe of this source
    for (size_t i = 0; i < source->num_universes; ++i)
      set_universe_terminating(&source->universes[i], kTerminateAndRemove);
  }
}

// Needs lock
void set_universe_terminating(SacnSourceUniverse* universe, set_terminating_behavior_t behavior)
{
  if (universe)
  {
    // Initialize the universe's termination state
    if (universe->termination_state == kNotTerminating)
      universe->num_terminations_sent = 0;

    switch (behavior)
    {
      case kTerminateAndRemove:
        universe->termination_state = kTerminatingAndRemoving;
        break;
      case kTerminateWithoutRemoving:
        if (universe->termination_state != kTerminatingAndRemoving)  // Continue removal if already in progress.
          universe->termination_state = kTerminatingWithoutRemoving;
        break;
    }

    // Set terminating for each unicast destination of this universe
    for (size_t i = 0; i < universe->num_unicast_dests; ++i)
      set_unicast_dest_terminating(&universe->unicast_dests[i], behavior);
  }
}

// Needs lock
void set_unicast_dest_terminating(SacnUnicastDestination* dest, set_terminating_behavior_t behavior)
{
  if (dest)
  {
    // Initialize the unicast destination's termination state
    if (dest->termination_state == kNotTerminating)
      dest->num_terminations_sent = 0;

    switch (behavior)
    {
      case kTerminateAndRemove:
        dest->termination_state = kTerminatingAndRemoving;
        break;
      case kTerminateWithoutRemoving:
        if (dest->termination_state != kTerminatingAndRemoving)  // Continue removal if already in progress.
          dest->termination_state = kTerminatingWithoutRemoving;
        break;
    }
  }
}

// Needs lock
void reset_transmission_suppression(const SacnSource* source, SacnSourceUniverse* universe,
                                    reset_transmission_suppression_behavior_t behavior)
{
  if ((behavior == kResetLevel) || (behavior == kResetLevelAndPap))
  {
    universe->level_packets_sent_before_suppression = 0;

    if (universe->has_level_data)
      etcpal_timer_start(&universe->level_keep_alive_timer, source->keep_alive_interval);
  }

  if ((behavior == kResetPap) || (behavior == kResetLevelAndPap))
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
  for (size_t i = 0; i < source->num_universes; ++i)
  {
    SacnSourceUniverse* universe = &source->universes[i];

    // Update the source name in this universe's send buffers
    strncpy((char*)(&universe->level_send_buf[SACN_SOURCE_NAME_OFFSET]), new_name, SACN_SOURCE_NAME_MAX_LEN);
    strncpy((char*)(&universe->pap_send_buf[SACN_SOURCE_NAME_OFFSET]), new_name, SACN_SOURCE_NAME_MAX_LEN);

    // Reset transmission suppression for start codes 0x00 and 0xDD
    reset_transmission_suppression(source, universe, kResetLevelAndPap);
  }
}

// Needs lock
size_t get_source_universes(const SacnSource* source, uint16_t* universes, size_t universes_size)
{
  size_t num_non_removed_universes = 0;
  for (size_t read = 0; (read < source->num_universes); ++read)
  {
    if (source->universes[read].termination_state != kTerminatingAndRemoving)
    {
      if (universes && (num_non_removed_universes < universes_size))
        universes[num_non_removed_universes] = source->universes[read].universe_id;

      ++num_non_removed_universes;
    }
  }

  return num_non_removed_universes;
}

// Needs lock
size_t get_source_unicast_dests(const SacnSourceUniverse* universe, EtcPalIpAddr* destinations,
                                size_t destinations_size)
{
  size_t num_non_removed_dests = 0;
  for (size_t read = 0; (read < universe->num_unicast_dests); ++read)
  {
    if (universe->unicast_dests[read].termination_state != kTerminatingAndRemoving)
    {
      if (destinations && (num_non_removed_dests < destinations_size))
        destinations[num_non_removed_dests] = universe->unicast_dests[read].dest_addr;

      ++num_non_removed_dests;
    }
  }

  return num_non_removed_dests;
}

// Needs lock
size_t get_source_universe_netints(const SacnSourceUniverse* universe, EtcPalMcastNetintId* netints,
                                   size_t netints_size)
{
  for (size_t i = 0; netints && (i < netints_size) && (i < universe->netints.num_netints); ++i)
    netints[i] = universe->netints.netints[i];

  return universe->netints.num_netints;
}

// Needs lock
void disable_pap_data(SacnSourceUniverse* universe)
{
  universe->has_pap_data = false;
}

// Needs lock
void clear_source_netints(SacnSource* source)
{
  source->num_netints = 0;  // Clear source netints, will be reconstructed when netints are re-added.
}

// Needs lock
etcpal_error_t reset_source_universe_networking(SacnSource* source, SacnSourceUniverse* universe,
                                                const SacnNetintConfig* netint_config)
{
  etcpal_error_t result = sacn_initialize_source_netints(&universe->netints, netint_config);

  for (size_t k = 0; (result == kEtcPalErrOk) && (k < universe->netints.num_netints); ++k)
    result = add_sacn_source_netint(source, &universe->netints.netints[k]);

  if (result == kEtcPalErrOk)
    reset_transmission_suppression(source, universe, kResetLevelAndPap);

  return result;
}

// Needs lock
void finish_source_universe_termination(SacnSource* source, size_t index)
{
  SacnSourceUniverse* universe = &source->universes[index];

  // Handle unicast destinations first
  size_t initial_num_unicast_dests = universe->num_unicast_dests;  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_unicast_dests; ++i)
    finish_unicast_dest_termination(universe, initial_num_unicast_dests - 1 - i);

  // Update num_active_universes if needed
  if (IS_PART_OF_UNIVERSE_DISCOVERY(universe))
    --source->num_active_universes;

  if (universe->termination_state == kTerminatingAndRemoving)
  {
    // Update the netints tree
    for (size_t i = 0; i < universe->netints.num_netints; ++i)
      remove_from_source_netints(source, &universe->netints.netints[i]);

    remove_sacn_source_universe(source, index);
  }
  else
  {
    reset_universe(universe);
  }
}

// Needs lock
void finish_unicast_dest_termination(SacnSourceUniverse* universe, size_t index)
{
  SacnUnicastDestination* dest = &universe->unicast_dests[index];

  if (dest->termination_state == kTerminatingAndRemoving)
    remove_sacn_unicast_dest(universe, index);
  else
    reset_unicast_dest(dest);
}

// Needs lock
void set_universe_priority(const SacnSource* source, SacnSourceUniverse* universe, uint8_t priority)
{
  universe->priority = priority;
  universe->level_send_buf[SACN_PRI_OFFSET] = priority;
  universe->pap_send_buf[SACN_PRI_OFFSET] = priority;
  reset_transmission_suppression(source, universe, kResetLevelAndPap);
}

// Needs lock
void set_preview_flag(const SacnSource* source, SacnSourceUniverse* universe, bool preview)
{
  universe->send_preview = preview;
  SET_PREVIEW_OPT(universe->level_send_buf, preview);
  SET_PREVIEW_OPT(universe->pap_send_buf, preview);
  reset_transmission_suppression(source, universe, kResetLevelAndPap);
}

void remove_from_source_netints(SacnSource* source, const EtcPalMcastNetintId* id)
{
  size_t netint_index = 0;
  SacnSourceNetint* netint_state = lookup_source_netint_and_index(source, id, &netint_index);

  if (netint_state)
  {
    if (netint_state->num_refs > 0)
      --netint_state->num_refs;

    if (netint_state->num_refs == 0)
      remove_sacn_source_netint(source, netint_index);
  }
}

// Needs lock
void reset_unicast_dest(SacnUnicastDestination* dest)
{
  dest->termination_state = kNotTerminating;
  dest->num_terminations_sent = 0;
}

// Needs lock
void reset_universe(SacnSourceUniverse* universe)
{
  universe->termination_state = kNotTerminating;
  universe->num_terminations_sent = 0;
  universe->has_level_data = false;
  universe->has_pap_data = false;
}

// Needs lock
void cancel_termination_if_not_removing(SacnSourceUniverse* universe)
{
  if (universe->termination_state == kTerminatingWithoutRemoving)
  {
    universe->termination_state = kNotTerminating;
    universe->num_terminations_sent = 0;

    for (size_t i = 0; i < universe->num_unicast_dests; ++i)
    {
      SacnUnicastDestination* dest = &universe->unicast_dests[i];

      if (dest->termination_state == kTerminatingWithoutRemoving)
      {
        dest->termination_state = kNotTerminating;
        dest->num_terminations_sent = 0;
      }
    }
  }
}

#endif  // SACN_SOURCE_ENABLED
