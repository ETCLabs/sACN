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

#include <assert.h>
#include <stdio.h>
#include "etcpal/common.h"
#include "etcpal/lock.h"
#include "etcpal/thread.h"
#include "etcpal/timer.h"
#include "sacn/receiver.h"

/* Disable strcpy() warning on MSVC */
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/*
 * This is a very simple sACN receiver example. It listens on sACN universes 1, 2, and 3 and prints
 * their activity periodically to the command line. This demonstrates use of the sACN Receiver API
 * and how to handle its callbacks.
 */

/**************************************************************************************************
 * Constants
 *************************************************************************************************/

#define NUM_LISTENERS 3
#define NUM_SOURCES_PER_LISTENER 4
#define NUM_SLOTS_DISPLAYED 10

#define PRINT_STATUS_INTERVAL 10000

/**************************************************************************************************
 * Global Data
 *************************************************************************************************/

/* We use these static structs to track data about up to 4 sources for each listening universe. */
typedef struct SourceData
{
  bool valid;
  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];
  uint8_t priority;
  int num_updates;
  uint8_t last_update[NUM_SLOTS_DISPLAYED];
} SourceData;

typedef struct ListeningUniverse
{
  sacn_receiver_t receiver_handle;
  uint16_t universe;
  SourceData sources[NUM_SOURCES_PER_LISTENER];
  size_t num_sources;
} ListeningUniverse;

ListeningUniverse listeners[NUM_LISTENERS];

etcpal_mutex_t mutex;

/**************************************************************************************************
 * Local Functions
 *************************************************************************************************/

static SourceData* find_source(ListeningUniverse* listener, const EtcPalUuid* cid)
{
  for (SourceData* source = listener->sources; source < listener->sources + NUM_SOURCES_PER_LISTENER; ++source)
  {
    if (source->valid && ETCPAL_UUID_CMP(&source->cid, cid) == 0)
      return source;
  }
  return NULL;
}

static SourceData* find_source_hole(ListeningUniverse* listener)
{
  for (SourceData* source = listener->sources; source < listener->sources + NUM_SOURCES_PER_LISTENER; ++source)
  {
    if (!source->valid)
      return source;
  }
  return NULL;
}

/* Print a status update for each listening universe about the status of its sources. */
void print_universe_updates(int interval_ms)
{
  if (etcpal_mutex_lock(&mutex))
  {
    printf("-------------------------------------------------------------------------------------------------------\n");
    for (ListeningUniverse* listener = listeners; listener < listeners + NUM_LISTENERS; ++listener)
    {
      printf("Universe %u currently tracking %zu sources:\n", listener->universe, listener->num_sources);
      for (SourceData* source = listener->sources; source < listener->sources + NUM_SOURCES_PER_LISTENER; ++source)
      {
        if (source->valid)
        {
          char cid_str[ETCPAL_UUID_STRING_BYTES];
          etcpal_uuid_to_string(&source->cid, cid_str);
          int update_rate = source->num_updates * 1000 / interval_ms;
          printf("  Source %s\tPriority: %u\tUpdates per second: %d\tLast update: ", cid_str, source->priority,
                 update_rate);
          for (size_t i = 0; i < NUM_SLOTS_DISPLAYED; ++i)
          {
            printf("%02x ", source->last_update[i]);
          }
          printf("Name: '%s'\n", source->name);
          source->num_updates = 0;
        }
      }
    }
    printf("-------------------------------------------------------------------------------------------------------\n");
    etcpal_mutex_unlock(&mutex);
  }
}

/**************************************************************************************************
 * sACN Callback Implementations
 *************************************************************************************************/

/*
 * This callback is called when a sACN data packet is received on a universe. In a real application
 * this data would be acted upon somehow. We just update some stats about the source of the data,
 * and store the first few slots.
 */
static void handle_universe_data(sacn_receiver_t handle, const EtcPalSockAddr* from_addr, const SacnHeaderData* header,
                                 const uint8_t* pdata, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(from_addr);

  if (etcpal_mutex_lock(&mutex))
  {
    ListeningUniverse* listener = (ListeningUniverse*)context;
    assert(listener);

    // See if we are already tracking this source.
    SourceData* source = find_source(listener, &header->cid);
    if (source)
    {
      ++source->num_updates;
      source->priority = header->priority;
      memcpy(source->last_update, pdata, NUM_SLOTS_DISPLAYED);
    }
    else
    {
      // See if there's room for new source data
      source = find_source_hole(listener);
      if (source)
      {
        source->cid = header->cid;
        strcpy(source->name, header->source_name);
        source->priority = header->priority;
        source->num_updates = 1;
        memcpy(source->last_update, pdata, NUM_SLOTS_DISPLAYED);
        source->valid = true;
        ++listener->num_sources;
      }
      else
      {
        printf("No room to track new source on universe %u\n", listener->universe);
      }
    }
    etcpal_mutex_unlock(&mutex);
  }
}

/*
 * This callback is called when one or more sources are lost on a universe. We remove them from our
 * tracking. In a real application this would be where any hold-last-look, etc. logic is
 * triggered for a universe.
 */
static void handle_sources_lost(sacn_receiver_t handle, const SacnLostSource* lost_sources, size_t num_lost_sources,
                                void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (etcpal_mutex_lock(&mutex))
  {
    ListeningUniverse* listener = (ListeningUniverse*)context;
    assert(listener);

    printf("Universe %u lost the following source(s):\n", listener->universe);
    for (const SacnLostSource* lost_source = lost_sources; lost_source < lost_sources + num_lost_sources; ++lost_source)
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&lost_source->cid, cid_str);
      printf("%s\t%s\tTerminated: %s\n", cid_str, lost_source->name, lost_source->terminated ? "true" : "false");

      SourceData* source = find_source(listener, &lost_source->cid);
      if (source)
      {
        source->valid = false;
        --listener->num_sources;
      }
    }
    etcpal_mutex_unlock(&mutex);
  }
}

/*
 * If sACN is compiled with SACN_ETC_PRIORITY_EXTENSION=1, this callback is called if a source was
 * sending per-channel priority packets and stopped sending them, but is still sending NULL start
 * code data.
 */
static void handle_source_pcp_lost(sacn_receiver_t handle, const SacnRemoteSource* source, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(context);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&source->cid, cid_str);
  printf("Per-channel priority lost for source '%s' (%s)\n", source->name, cid_str);
}

static void handle_sampling_ended(sacn_receiver_t handle, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  ListeningUniverse* listener = (ListeningUniverse*)context;
  printf("Sampling ended for universe %u\n", listener->universe);
}

static void handle_source_limit_exceeded(sacn_receiver_t handle, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  ListeningUniverse* listener = (ListeningUniverse*)context;
  printf("Source limit exceeded on universe %u\n", listener->universe);
}

// clang-format off
static const SacnReceiverCallbacks kSacnCallbacks = {
  handle_universe_data,
  handle_sources_lost,
  handle_source_pcp_lost,
  handle_sampling_ended,
  handle_source_limit_exceeded
};
// clang-format on

static void log_callback(void* context, const EtcPalLogStrings* strings)
{
  ETCPAL_UNUSED_ARG(context);
  printf("%s\n", strings->human_readable);
}

/**************************************************************************************************
 * Keyboard Interrupt Handling
 *************************************************************************************************/

bool keep_running = true;

static void handle_keyboard_interrupt()
{
  keep_running = false;
}

extern void install_keyboard_interrupt_handler(void (*handler)());

int main(void)
{
  if (!etcpal_mutex_create(&mutex))
  {
    printf("Couldn't create mutex!\n");
    return 1;
  }

  // Initialize the sACN library, allowing it to log messages through our callback
  EtcPalLogParams log_params;
  log_params.action = kEtcPalLogCreateHumanReadable;
  log_params.log_fn = log_callback;
  log_params.time_fn = NULL;
  log_params.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);

  etcpal_error_t result = sacn_init(&log_params);
  if (result != kEtcPalErrOk)
  {
    printf("sACN initialization failed with error: '%s'\n", etcpal_strerror(result));
    return 1;
  }

  // Start listening on each universe starting at 1
  uint16_t universe = 1;
  for (ListeningUniverse* listener = listeners; listener < listeners + NUM_LISTENERS; ++listener)
  {
    SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
    config.callbacks = kSacnCallbacks;
    config.callback_context = listener;
    config.universe_id = universe;

    result = sacn_receiver_create(&config, &listener->receiver_handle);
    if (result == kEtcPalErrOk)
    {
      listener->universe = universe++;
      listener->num_sources = 0;
    }
    else
    {
      printf("Creating sACN receiver failed with error: '%s'\n", etcpal_strerror(result));
      sacn_deinit();
      return 1;
    }
  }

  // Handle Ctrl+C gracefully and shut down in compatible consoles
  install_keyboard_interrupt_handler(handle_keyboard_interrupt);

  // Print status info periodically until we are told to stop
  EtcPalTimer timer;
  etcpal_timer_start(&timer, PRINT_STATUS_INTERVAL);
  while (keep_running)
  {
    etcpal_thread_sleep(50);
    if (etcpal_timer_is_expired(&timer))
    {
      print_universe_updates(PRINT_STATUS_INTERVAL);
      etcpal_timer_reset(&timer);
    }
  }

  printf("Shutting down sACN...\n");

  for (ListeningUniverse* listener = listeners; listener < listeners + NUM_LISTENERS; ++listener)
  {
    etcpal_error_t destroy_res = sacn_receiver_destroy(listener->receiver_handle);
    if (destroy_res != kEtcPalErrOk)
    {
      printf("Error destroying sACN receiver %d: '%s'!\n", listener->receiver_handle, etcpal_strerror(destroy_res));
    }
  }

  sacn_deinit();
  etcpal_mutex_destroy(&mutex);
  return 0;
}
