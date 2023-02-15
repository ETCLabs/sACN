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

static void sleep_until_time_elapsed(const EtcPalTimer* timer, uint32_t target_elapsed_ms);
static void source_thread_function(void* arg);

static int process_sources(process_sources_behavior_t behavior, sacn_source_tick_mode_t tick_mode);
static bool process_universe_discovery(SacnSource* source);
static bool process_universes(SacnSource* source, sacn_source_tick_mode_t tick_mode);
static void process_stats_log(SacnSource* source, bool all_sends_succeeded);
static bool process_unicast_termination(SacnSource* source, SacnSourceUniverse* universe, bool* terminating);
static bool process_multicast_termination(SacnSource* source, size_t index, bool unicast_terminating);
static bool transmit_levels_and_pap_when_needed(SacnSource* source, SacnSourceUniverse* universe,
                                                sacn_source_tick_mode_t tick_mode);
static bool send_termination_multicast(const SacnSource* source, SacnSourceUniverse* universe);
static bool send_termination_unicast(const SacnSource* source, SacnSourceUniverse* universe,
                                     SacnUnicastDestination* dest);
static bool send_universe_discovery(SacnSource* source);
static int pack_universe_discovery_page(SacnSource* source, size_t* total_universes_processed, uint8_t page_number);
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

static void handle_data_packet_sent(const uint8_t* send_buf, SacnSourceUniverse* universe);

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

void sleep_until_time_elapsed(const EtcPalTimer* timer, uint32_t target_elapsed_ms)
{
  uint32_t elapsed_ms = etcpal_timer_elapsed(timer);
  while (elapsed_ms < target_elapsed_ms)
  {
    etcpal_thread_sleep(target_elapsed_ms - elapsed_ms);
    elapsed_ms = etcpal_timer_elapsed(timer);
  }
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
    // Space out sending of levels & PAP as follows:
    // |------------------------------- 23ms -------------------------------|
    // |--- Send Levels ---|              |--- Send PAP ---|
    //
    // This is to help reduce packet dropping when sending hundreds of universes.
    take_lock_and_process_sources(kProcessThreadedSources, kSacnSourceTickModeProcessLevelsOnly);

    sleep_until_time_elapsed(&interval_timer, SOURCE_THREAD_INTERVAL / 2);

    num_thread_based_sources =
        take_lock_and_process_sources(kProcessThreadedSources, kSacnSourceTickModeProcessPapOnly);

    sleep_until_time_elapsed(&interval_timer, SOURCE_THREAD_INTERVAL);
    etcpal_timer_reset(&interval_timer);

    if (sacn_lock())
    {
      keep_running_thread = !shutting_down;
      sacn_unlock();
    }
  }
}

// Takes lock
int take_lock_and_process_sources(process_sources_behavior_t behavior, sacn_source_tick_mode_t tick_mode)
{
  int num_sources_tracked = 0;

  if (sacn_lock())
  {
    num_sources_tracked = process_sources(behavior, tick_mode);
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
int process_sources(process_sources_behavior_t behavior, sacn_source_tick_mode_t tick_mode)
{
  int num_sources_tracked = 0;

  size_t initial_num_sources = get_num_sources();  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_sources; ++i)
  {
    SacnSource* source = get_source(initial_num_sources - 1 - i);
    if (SACN_ASSERT_VERIFY(source))
    {
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
        bool all_sends_succeeded = process_universe_discovery(source) && process_universes(source, tick_mode);
        process_stats_log(source, all_sends_succeeded);

        // Clean up this source if needed
        if (source->terminating && (source->num_universes == 0))
          remove_sacn_source(initial_num_sources - 1 - i);
      }
    }
  }

  return num_sources_tracked;
}

// Needs lock
bool process_universe_discovery(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return false;

  // Send another universe discovery packet if it's time
  bool all_sends_succeeded = true;
  if (!source->terminating && etcpal_timer_is_expired(&source->universe_discovery_timer))
  {
    all_sends_succeeded = send_universe_discovery(source);
    etcpal_timer_reset(&source->universe_discovery_timer);
  }

  return all_sends_succeeded;
}

// Needs lock
bool process_universes(SacnSource* source, sacn_source_tick_mode_t tick_mode)
{
  if (!SACN_ASSERT_VERIFY(source))
    return false;

  bool all_sends_succeeded = true;

  size_t initial_num_universes = source->num_universes;  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_universes; ++i)
  {
    SacnSourceUniverse* universe = &source->universes[initial_num_universes - 1 - i];

    // Unicast destination-specific processing
    bool unicast_terminating = false;
    if (tick_mode != kSacnSourceTickModeProcessPapOnly)  // Only do termination if processing levels
      all_sends_succeeded = process_unicast_termination(source, universe, &unicast_terminating);

    // Either transmit start codes 0x00 and/or 0xDD, or terminate and clean up universe
    if (universe->termination_state == kNotTerminating)
    {
      all_sends_succeeded = all_sends_succeeded && transmit_levels_and_pap_when_needed(source, universe, tick_mode);
    }
    else if (tick_mode != kSacnSourceTickModeProcessPapOnly)  // Only do termination if processing levels
    {
      all_sends_succeeded = all_sends_succeeded &&
                            process_multicast_termination(source, initial_num_universes - 1 - i, unicast_terminating);
    }

    increment_sequence_number(universe);
  }

  return all_sends_succeeded;
}

// Needs lock
void process_stats_log(SacnSource* source, bool all_sends_succeeded)
{
  if (!SACN_ASSERT_VERIFY(source))
    return;

  ++source->total_tick_count;
  if (!all_sends_succeeded)
    ++source->failed_tick_count;

  if (etcpal_timer_is_expired(&source->stats_log_timer))
  {
#if SACN_LOGGING_ENABLED
    if ((source->failed_tick_count > 0) && SACN_ASSERT_VERIFY(source->total_tick_count >= source->failed_tick_count) &&
        SACN_CAN_LOG(ETCPAL_LOG_INFO))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&source->cid, cid_str);

      double failed_tick_ratio = (double)source->failed_tick_count / (double)source->total_tick_count;

      SACN_LOG_INFO("In the last %d seconds, source %s had %d out of %d ticks (%f%%) fail at least one send.",
                    SACN_STATS_LOG_INTERVAL / 1000, cid_str, source->failed_tick_count, source->total_tick_count,
                    failed_tick_ratio * 100.0);
    }
#endif  // SACN_LOGGING_ENABLED

    etcpal_timer_reset(&source->stats_log_timer);
    source->total_tick_count = 0;
    source->failed_tick_count = 0;
  }
}

// Needs lock
bool process_unicast_termination(SacnSource* source, SacnSourceUniverse* universe, bool* terminating)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe) || !SACN_ASSERT_VERIFY(terminating))
    return false;

  bool all_sends_succeeded = true;

  *terminating = false;

  size_t initial_num_unicast_dests = universe->num_unicast_dests;  // Actual may change, so keep initial for iteration.
  for (size_t i = 0; i < initial_num_unicast_dests; ++i)
  {
    SacnUnicastDestination* dest = &universe->unicast_dests[initial_num_unicast_dests - 1 - i];

    // Terminate and clean up this unicast destination if needed
    if (dest->termination_state != kNotTerminating)
    {
      if ((dest->num_terminations_sent < 3) && universe->has_level_data)
        all_sends_succeeded = all_sends_succeeded && send_termination_unicast(source, universe, dest);

      if ((dest->num_terminations_sent >= 3) || !universe->has_level_data)
        finish_unicast_dest_termination(universe, initial_num_unicast_dests - 1 - i);
      else
        *terminating = true;
    }
  }

  return all_sends_succeeded;
}

// Needs lock
bool process_multicast_termination(SacnSource* source, size_t index, bool unicast_terminating)
{
  if (!SACN_ASSERT_VERIFY(source))
    return false;

  bool all_sends_succeeded = true;

  SacnSourceUniverse* universe = &source->universes[index];

  if ((universe->num_terminations_sent < 3) && universe->has_level_data)
    all_sends_succeeded = send_termination_multicast(source, universe);

  if (((universe->num_terminations_sent >= 3) && !unicast_terminating) || !universe->has_level_data)
    finish_source_universe_termination(source, index);

  return all_sends_succeeded;
}

// Needs lock
bool transmit_levels_and_pap_when_needed(SacnSource* source, SacnSourceUniverse* universe,
                                         sacn_source_tick_mode_t tick_mode)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return false;

  bool can_process_levels =
      (tick_mode == kSacnSourceTickModeProcessLevelsOnly) || (tick_mode == kSacnSourceTickModeProcessLevelsAndPap);
  bool can_process_pap =
      (tick_mode == kSacnSourceTickModeProcessPapOnly) || (tick_mode == kSacnSourceTickModeProcessLevelsAndPap);

  bool all_sends_succeeded = true;

  // If 0x00 data is ready to send
  if (can_process_levels && universe->has_level_data &&
      ((universe->level_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS) ||
       etcpal_timer_is_expired(&universe->level_keep_alive_timer)))
  {
    // Send 0x00 data & reset the keep-alive timer
    all_sends_succeeded = send_universe_multicast(source, universe, universe->level_send_buf);
    all_sends_succeeded = all_sends_succeeded && send_universe_unicast(source, universe, universe->level_send_buf);

    if (universe->level_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS)
      ++universe->level_packets_sent_before_suppression;

    etcpal_timer_reset(&universe->level_keep_alive_timer);
  }
#if SACN_ETC_PRIORITY_EXTENSION
  // If 0xDD data is ready to send
  if (can_process_pap && universe->has_pap_data &&
      ((universe->pap_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS) ||
       etcpal_timer_is_expired(&universe->pap_keep_alive_timer)))
  {
    // PAP will always be sent after levels, so if levels were sent this tick, PAP's seq_num should be one greater.
    // This is the only place where we can determine whether or not we need to do this prior to sending PAP. If we do,
    // the increment_sequence_number function will know to increment next_seq_num by 2 based on the send flags.
    // Likewise, if we don't, then it'll increment by 1.
    if (universe->levels_sent_this_tick)
      pack_sequence_number(universe->pap_send_buf, universe->next_seq_num + 1);

    // Send 0xDD data & reset the keep-alive timer
    all_sends_succeeded = all_sends_succeeded && send_universe_multicast(source, universe, universe->pap_send_buf);
    all_sends_succeeded = all_sends_succeeded && send_universe_unicast(source, universe, universe->pap_send_buf);

    if (universe->pap_packets_sent_before_suppression < NUM_PRE_SUPPRESSION_PACKETS)
      ++universe->pap_packets_sent_before_suppression;

    etcpal_timer_reset(&universe->pap_keep_alive_timer);
  }
#endif

  return all_sends_succeeded;
}

// Needs lock
void pack_sequence_number(uint8_t* buf, uint8_t seq_num)
{
  if (!SACN_ASSERT_VERIFY(buf))
    return;

  buf[SACN_SEQ_OFFSET] = seq_num;
}

// Needs lock
void increment_sequence_number(SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(universe))
    return;

  // Either one or both of levels & PAP were sent, or a different start code was sent via send_now.
  if (universe->levels_sent_this_tick)
  {
    ++universe->next_seq_num;
    universe->levels_sent_this_tick = false;
  }
#if SACN_ETC_PRIORITY_EXTENSION
  if (universe->pap_sent_this_tick)
  {
    ++universe->next_seq_num;
    universe->pap_sent_this_tick = false;
  }
#endif
  if (universe->other_sent_this_tick)
  {
    ++universe->next_seq_num;
    universe->other_sent_this_tick = false;
  }

  universe->anything_sent_this_tick = false;

  // Pack defaults for the next tick (PAP's seq_num will end up being one higher if sent in the same tick as levels)
  pack_sequence_number(universe->level_send_buf, universe->next_seq_num);
#if SACN_ETC_PRIORITY_EXTENSION
  pack_sequence_number(universe->pap_send_buf, universe->next_seq_num);
#endif
}

// Needs lock
bool send_termination_multicast(const SacnSource* source, SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return false;

  // Repurpose level_send_buf for the termination packet
  bool old_terminated_opt = TERMINATED_OPT_SET(universe->level_send_buf);
  SET_TERMINATED_OPT(universe->level_send_buf, true);

  // Send the termination packet on multicast only
  bool all_sends_succeeded = send_universe_multicast(source, universe, universe->level_send_buf);

  // Increment the termination counter
  ++universe->num_terminations_sent;

  // Revert terminated flag
  SET_TERMINATED_OPT(universe->level_send_buf, old_terminated_opt);

  return all_sends_succeeded;
}

// Needs lock
bool send_termination_unicast(const SacnSource* source, SacnSourceUniverse* universe, SacnUnicastDestination* dest)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe) || !SACN_ASSERT_VERIFY(dest))
    return false;

  // Repurpose level_send_buf for the termination packet
  bool old_terminated_opt = TERMINATED_OPT_SET(universe->level_send_buf);
  SET_TERMINATED_OPT(universe->level_send_buf, true);

  // Send the termination packet on unicast only
  bool res = true;
  if (sacn_send_unicast(source->ip_supported, universe->level_send_buf, &dest->dest_addr, &dest->last_send_error) ==
      kEtcPalErrOk)
  {
    handle_data_packet_sent(universe->level_send_buf, universe);
  }
  else
  {
    res = false;
  }

  ++dest->num_terminations_sent;

  // Revert terminated flag
  SET_TERMINATED_OPT(universe->level_send_buf, old_terminated_opt);

  return res;
}

// Needs lock
bool send_universe_discovery(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return false;

  bool all_sends_succeeded = true;

  // If there are network interfaces to send on
  if (source->num_netints > 0)
  {
    // Initialize universe & page counters
    size_t total_universes_processed = 0;
    uint8_t page_number = 0;

    // Pack the next page & loop while there's a page to send
    while (pack_universe_discovery_page(source, &total_universes_processed, page_number) > 0)
    {
      // Send multicast on IPv4 and/or IPv6
      bool at_least_one_send_worked = false;
      for (size_t i = 0; i < source->num_netints; ++i)
      {
        if (sacn_send_multicast(SACN_DISCOVERY_UNIVERSE, source->ip_supported, source->universe_discovery_send_buf,
                                &source->netints[i].id) == kEtcPalErrOk)
        {
          at_least_one_send_worked = true;
        }
        else
        {
          all_sends_succeeded = false;
        }
      }

      if (at_least_one_send_worked)
        ++page_number;
      else
        break;
    }
  }

  return all_sends_succeeded;
}

// Needs lock
bool send_universe_multicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe) || !SACN_ASSERT_VERIFY(send_buf))
    return false;

  bool at_least_one_sent = false;
  bool all_sends_succeeded = true;
  if (!universe->send_unicast_only)
  {
    for (size_t i = 0; i < universe->netints.num_netints; ++i)
    {
      etcpal_error_t send_res =
          sacn_send_multicast(universe->universe_id, source->ip_supported, send_buf, &universe->netints.netints[i]);
      if (send_res == kEtcPalErrOk)
      {
        at_least_one_sent = true;
      }
      else
      {
        all_sends_succeeded = false;
        universe->last_send_error = send_res;
      }
    }
  }

  if (at_least_one_sent)
    handle_data_packet_sent(send_buf, universe);

  return all_sends_succeeded;
}

// Needs lock
bool send_universe_unicast(const SacnSource* source, SacnSourceUniverse* universe, const uint8_t* send_buf)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe) || !SACN_ASSERT_VERIFY(send_buf))
    return false;

  bool at_least_one_sent = false;
  bool all_sends_succeeded = true;
  for (size_t i = 0; i < universe->num_unicast_dests; ++i)
  {
    if (universe->unicast_dests[i].termination_state == kNotTerminating)
    {
      etcpal_error_t send_res = sacn_send_unicast(source->ip_supported, send_buf, &universe->unicast_dests[i].dest_addr,
                                                  &universe->unicast_dests[i].last_send_error);
      if (send_res == kEtcPalErrOk)
      {
        at_least_one_sent = true;
      }
      else
      {
        all_sends_succeeded = false;
        universe->last_send_error = send_res;
      }
    }
  }

  if (at_least_one_sent)
    handle_data_packet_sent(send_buf, universe);

  return all_sends_succeeded;
}

// Needs lock
int pack_universe_discovery_page(SacnSource* source, size_t* total_universes_processed, uint8_t page_number)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(total_universes_processed))
    return 0;

  // Initialize packing pointer and universe counter
  uint8_t* pcur = &source->universe_discovery_send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE];
  int num_universes_packed = 0;

  // Iterate up to 512 universes
  while ((*total_universes_processed < source->num_universes) &&
         (num_universes_packed < SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE))
  {
    // Iterate universes array in reverse to pack universes lowest to highest
    size_t index = (source->num_universes - 1) - (*total_universes_processed);
    const SacnSourceUniverse* universe = &source->universes[index];

    // If this universe has level data at a bare minimum & is not unicast-only
    if (IS_PART_OF_UNIVERSE_DISCOVERY(universe))
    {
      // Pack the universe ID
      etcpal_pack_u16b(pcur, universe->universe_id);
      pcur += 2;

      // Increment number of universes packed
      ++num_universes_packed;
    }

    ++(*total_universes_processed);
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
  if (!SACN_ASSERT_VERIFY(source_state) || !SACN_ASSERT_VERIFY(universe_state) || !SACN_ASSERT_VERIFY(new_levels) ||
      !SACN_ASSERT_VERIFY(new_levels_size > 0))
  {
    return;
  }

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
  if (!SACN_ASSERT_VERIFY(source_state) || !SACN_ASSERT_VERIFY(universe_state) || !SACN_ASSERT_VERIFY(new_priorities) ||
      !SACN_ASSERT_VERIFY(new_priorities_size > 0))
  {
    return;
  }

  update_send_buf_data(universe_state->pap_send_buf, new_priorities, (uint16_t)new_priorities_size, force_sync);
  universe_state->has_pap_data = true;
  reset_transmission_suppression(source_state, universe_state, kResetPap);
}

// Needs lock
void zero_levels_where_pap_is_zero(SacnSourceUniverse* universe_state)
{
  if (!SACN_ASSERT_VERIFY(universe_state))
    return;

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
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return;

#if SACN_ETC_PRIORITY_EXTENSION
  // Make sure PAP is updated before levels.
  if (new_priorities)
    update_pap(source, universe, new_priorities, new_priorities_size, force_sync);
#endif
  if (new_levels)
    update_levels(source, universe, new_levels, new_levels_size, force_sync);
}

// Needs lock
void set_source_terminating(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return;

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
  if (!SACN_ASSERT_VERIFY(universe))
    return;

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

// Needs lock
void set_unicast_dest_terminating(SacnUnicastDestination* dest, set_terminating_behavior_t behavior)
{
  if (!SACN_ASSERT_VERIFY(dest))
    return;

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

// Needs lock
void reset_transmission_suppression(const SacnSource* source, SacnSourceUniverse* universe,
                                    reset_transmission_suppression_behavior_t behavior)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return;

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
      etcpal_timer_start(&universe->pap_keep_alive_timer, source->pap_keep_alive_interval);
  }
}

// Needs lock
void set_source_name(SacnSource* source, const char* new_name)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(new_name))
    return;

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
  if (!SACN_ASSERT_VERIFY(source))
    return 0;

  // Universes array is sorted highest to lowest - iterate in reverse to output lowest to highest.
  size_t num_non_removed_universes = 0;
  for (int read = (int)source->num_universes - 1; read >= 0; --read)
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
  if (!SACN_ASSERT_VERIFY(universe))
    return 0;

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
  if (!SACN_ASSERT_VERIFY(universe))
    return 0;

  for (size_t i = 0; netints && (i < netints_size) && (i < universe->netints.num_netints); ++i)
    netints[i] = universe->netints.netints[i];

  return universe->netints.num_netints;
}

// Needs lock
void disable_pap_data(SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(universe))
    return;

  universe->has_pap_data = false;
}

// Needs lock
void clear_source_netints(SacnSource* source)
{
  if (!SACN_ASSERT_VERIFY(source))
    return;

  source->num_netints = 0;  // Clear source netints, will be reconstructed when netints are re-added.
}

// Needs lock
etcpal_error_t reset_source_universe_networking(SacnSource* source, SacnSourceUniverse* universe,
                                                const SacnNetintConfig* netint_config)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return kEtcPalErrSys;

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
  if (!SACN_ASSERT_VERIFY(source))
    return;

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
  if (!SACN_ASSERT_VERIFY(universe))
    return;

  SacnUnicastDestination* dest = &universe->unicast_dests[index];

  if (dest->termination_state == kTerminatingAndRemoving)
    remove_sacn_unicast_dest(universe, index);
  else
    reset_unicast_dest(dest);
}

// Needs lock
void set_universe_priority(const SacnSource* source, SacnSourceUniverse* universe, uint8_t priority)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return;

  universe->priority = priority;
  universe->level_send_buf[SACN_PRI_OFFSET] = priority;
  universe->pap_send_buf[SACN_PRI_OFFSET] = priority;
  reset_transmission_suppression(source, universe, kResetLevelAndPap);
}

// Needs lock
void set_preview_flag(const SacnSource* source, SacnSourceUniverse* universe, bool preview)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(universe))
    return;

  universe->send_preview = preview;
  SET_PREVIEW_OPT(universe->level_send_buf, preview);
  SET_PREVIEW_OPT(universe->pap_send_buf, preview);
  reset_transmission_suppression(source, universe, kResetLevelAndPap);
}

void remove_from_source_netints(SacnSource* source, const EtcPalMcastNetintId* id)
{
  if (!SACN_ASSERT_VERIFY(source) || !SACN_ASSERT_VERIFY(id))
    return;

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
  if (!SACN_ASSERT_VERIFY(dest))
    return;

  dest->termination_state = kNotTerminating;
  dest->num_terminations_sent = 0;
}

// Needs lock
void reset_universe(SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(universe))
    return;

  universe->termination_state = kNotTerminating;
  universe->num_terminations_sent = 0;
  universe->has_level_data = false;
  universe->has_pap_data = false;
}

// Needs lock
void cancel_termination_if_not_removing(SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(universe))
    return;

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

// Needs lock
void handle_data_packet_sent(const uint8_t* send_buf, SacnSourceUniverse* universe)
{
  if (!SACN_ASSERT_VERIFY(send_buf) || !SACN_ASSERT_VERIFY(universe))
    return;

  // The assertions below enforce assumptions made by the sequence number logic - specifically that only levels & PAP
  // can be sent in combination, and also that PAP is sent last each tick.
  if (send_buf[SACN_START_CODE_OFFSET] == SACN_STARTCODE_DMX)
  {
    SACN_ASSERT_VERIFY(!universe->other_sent_this_tick);
#if SACN_ETC_PRIORITY_EXTENSION
    SACN_ASSERT_VERIFY(!universe->pap_sent_this_tick);
#endif
    universe->levels_sent_this_tick = true;
  }
#if SACN_ETC_PRIORITY_EXTENSION
  else if (send_buf[SACN_START_CODE_OFFSET] == SACN_STARTCODE_PRIORITY)
  {
    SACN_ASSERT_VERIFY(!universe->other_sent_this_tick);
    universe->pap_sent_this_tick = true;
  }
#endif
  else
  {
    SACN_ASSERT_VERIFY(!universe->levels_sent_this_tick);
#if SACN_ETC_PRIORITY_EXTENSION
    SACN_ASSERT_VERIFY(!universe->pap_sent_this_tick);
#endif
    universe->other_sent_this_tick = true;
  }

  universe->anything_sent_this_tick = true;
}

#endif  // SACN_SOURCE_ENABLED
