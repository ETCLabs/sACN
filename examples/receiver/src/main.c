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

#include <assert.h>
#include <stdio.h>
#include "etcpal/common.h"
#include "etcpal/mutex.h"
#include "etcpal/thread.h"
#include "etcpal/timer.h"
#include "sacn/receiver.h"

/* Disable strcpy() warning on MSVC */
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/*
 * This is a very simple sACN receiver example. The user can create and destroy receivers or change the universes of
 * existing receivers. They can also print the network values. This demonstrates use of the sACN Receiver API and how to
 * handle its callbacks.
 */

/**************************************************************************************************
 * Constants
 *************************************************************************************************/

#define MAX_LISTENERS 10
#define NUM_SOURCES_PER_LISTENER 4
#define NUM_SLOTS_DISPLAYED 10
#define BEGIN_BORDER_STRING \
  ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"
#define END_BORDER_STRING \
  "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n"

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
  uint32_t update_start_time_ms;
  uint8_t last_update[NUM_SLOTS_DISPLAYED];
} SourceData;

typedef struct ListeningUniverse
{
  sacn_receiver_t receiver_handle;
  uint16_t universe;
  SourceData sources[NUM_SOURCES_PER_LISTENER];
  size_t num_sources;
} ListeningUniverse;

ListeningUniverse listeners[MAX_LISTENERS];

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

static void invalidate_listeners()
{
  for (ListeningUniverse* listener = listeners; listener < listeners + MAX_LISTENERS; ++listener)
  {
    listener->receiver_handle = SACN_RECEIVER_INVALID;
  }
}

static void invalidate_sources(ListeningUniverse* listener)
{
  if (listener)
  {
    for (SourceData* source = listener->sources; source < listener->sources + NUM_SOURCES_PER_LISTENER; ++source)
    {
      source->valid = false;
    }

    listener->num_sources = 0;
  }
}

static ListeningUniverse* find_listener_hole()
{
  for (ListeningUniverse* listener = listeners; listener < listeners + MAX_LISTENERS; ++listener)
  {
    if (listener->receiver_handle == SACN_RECEIVER_INVALID)
    {
      return listener;
    }
  }

  return NULL;
}

static ListeningUniverse* find_listener_on_universe(uint16_t universe)
{
  for (ListeningUniverse* listener = listeners; listener < listeners + MAX_LISTENERS; ++listener)
  {
    if (listener->universe == universe)
    {
      return listener;
    }
  }

  return NULL;
}

static etcpal_error_t create_listener(ListeningUniverse* listener, uint16_t universe,
                                      const SacnReceiverCallbacks* callbacks)
{
  SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
  config.callbacks = *callbacks;
  config.callbacks.context = listener;
  config.universe_id = universe;

  printf("Creating a new sACN receiver on universe %u.\n", universe);

  // Normally passing in NULL and 0 for netints and length would achieve the same result, this is just for
  // demonstration.
  size_t num_sys_netints = etcpal_netint_get_num_interfaces();
  const EtcPalNetintInfo* netint_list = etcpal_netint_get_interfaces();
#define MAX_LISTENER_NETINTS 100
  SacnMcastInterface netints[MAX_LISTENER_NETINTS];

  for (size_t i = 0; (i < num_sys_netints) && (i < MAX_LISTENER_NETINTS); ++i)
  {
    netints[i].iface.index = netint_list[i].index;
    netints[i].iface.ip_type = netint_list[i].addr.type;
  }

  SacnNetintConfig netint_config;
  netint_config.netints = netints;
  netint_config.num_netints = (num_sys_netints < MAX_LISTENER_NETINTS) ? num_sys_netints : MAX_LISTENER_NETINTS;

  etcpal_error_t result = sacn_receiver_create(&config, &listener->receiver_handle, &netint_config);
  if (result == kEtcPalErrOk)
  {
    listener->universe = universe;
    listener->num_sources = 0;
  }
  else
  {
    printf("Creating sACN receiver failed with error: '%s'\n", etcpal_strerror(result));
  }

  return result;
}

static etcpal_error_t destroy_listener(ListeningUniverse* listener)
{
  printf("Destroying sACN receiver %d.\n", listener->receiver_handle);
  etcpal_error_t destroy_res = sacn_receiver_destroy(listener->receiver_handle);
  if (destroy_res == kEtcPalErrOk)
  {
    invalidate_sources(listener);
    listener->receiver_handle = SACN_RECEIVER_INVALID;
  }
  else
  {
    printf("Error destroying sACN receiver %d: '%s'!\n", listener->receiver_handle, etcpal_strerror(destroy_res));
  }

  return destroy_res;
}

static etcpal_error_t recreate_listener(ListeningUniverse* listener, uint16_t universe,
                                        const SacnReceiverCallbacks* callbacks)
{
  etcpal_error_t recreate_result = destroy_listener(listener);
  if (recreate_result == kEtcPalErrOk)
  {
    recreate_result = create_listener(listener, universe, callbacks);
  }

  return recreate_result;
}

static etcpal_error_t update_listener_universe(ListeningUniverse* listener, uint16_t new_universe,
                                               const SacnReceiverCallbacks* callbacks)
{
  etcpal_error_t result_to_return = kEtcPalErrOk;

  printf("Changing sACN receiver %d from universe %u to universe %u.\n", listener->receiver_handle, listener->universe,
         new_universe);
  etcpal_error_t change_result = sacn_receiver_change_universe(listener->receiver_handle, new_universe);

  if (change_result == kEtcPalErrOk)
  {
    listener->universe = new_universe;
    invalidate_sources(listener);
  }
  else
  {
    printf("Changing receiver universe failed with error: '%s'\n", etcpal_strerror(change_result));
    result_to_return = recreate_listener(listener, listener->universe, callbacks);

    if (result_to_return == kEtcPalErrOk)
    {
      printf("Successfully recreated receiver at universe %u.", listener->universe);
    }
    else
    {
      printf("Recreating receiver at universe %u failed with error: '%s'\n", listener->universe,
             etcpal_strerror(result_to_return));
    }
  }

  return result_to_return;
}

static void console_print_help()
{
  printf(BEGIN_BORDER_STRING);
  printf("Each input is listed followed by the action:\n");
  printf("h : Print help.\n");
  printf("p : Print updates for all receivers.\n");
  printf("a : Add a new receiver.\n");
  printf("r : Remove a receiver.\n");
  printf("c : Change a receiver's universe.\n");
  printf("ctrl-c : Exit.\n");
  printf(END_BORDER_STRING);
}

/* Print a status update for each listening universe about the status of its sources. */
static void console_print_universe_updates()
{
  if (etcpal_mutex_lock(&mutex))
  {
    printf(BEGIN_BORDER_STRING);
    for (ListeningUniverse* listener = listeners; listener < listeners + MAX_LISTENERS; ++listener)
    {
      if (listener->receiver_handle != SACN_RECEIVER_INVALID)
      {
        printf("Receiver %d on universe %u currently tracking %zu sources:\n", listener->receiver_handle,
               listener->universe, listener->num_sources);
        for (SourceData* source = listener->sources; source < listener->sources + NUM_SOURCES_PER_LISTENER; ++source)
        {
          if (source->valid)
          {
            char cid_str[ETCPAL_UUID_STRING_BYTES];
            etcpal_uuid_to_string(&source->cid, cid_str);
            uint32_t interval_ms = etcpal_getms() - source->update_start_time_ms;
            int update_rate = source->num_updates * 1000 / interval_ms;
            printf("  Source %s\tPriority: %u\tUpdates per second: %d\tLast update: ", cid_str, source->priority,
                   update_rate);
            for (size_t i = 0; i < NUM_SLOTS_DISPLAYED; ++i)
            {
              printf("%02x ", source->last_update[i]);
            }
            printf("Name: '%s'\n", source->name);
            source->num_updates = 0;
            source->update_start_time_ms = etcpal_getms();
          }
        }
      }
    }
    printf(END_BORDER_STRING);
    etcpal_mutex_unlock(&mutex);
  }
}

static etcpal_error_t console_add_listening_universe(const SacnReceiverCallbacks* callbacks)
{
  etcpal_error_t result = kEtcPalErrOk;
  ListeningUniverse* new_listener = find_listener_hole();

  printf(BEGIN_BORDER_STRING);
  if (new_listener)
  {
    uint16_t universe = 0;
    printf("Enter the universe number:\n");
    scanf("%hu", &universe);
    result = create_listener(new_listener, universe, callbacks);
    printf("Result: %s\n", etcpal_strerror(result));
  }
  else
  {
    printf("Maximum number of receivers has been reached. Please remove a receiver first before adding a new one.\n");
  }

  printf(END_BORDER_STRING);

  return result;
}

static etcpal_error_t console_remove_listening_universe()
{
  etcpal_error_t result = kEtcPalErrOk;

  uint16_t universe = 0;
  printf(BEGIN_BORDER_STRING);
  printf("Enter the universe number:\n");
  scanf("%hu", &universe);

  ListeningUniverse* listener = find_listener_on_universe(universe);
  if (listener)
  {
    result = destroy_listener(listener);
    printf("Result: %s\n", etcpal_strerror(result));
  }
  else
  {
    printf("There are no receivers currently listening to universe %hu.\n", universe);
  }

  printf(END_BORDER_STRING);

  return result;
}

static etcpal_error_t console_change_listening_universe(const SacnReceiverCallbacks* callbacks)
{
  etcpal_error_t result = kEtcPalErrOk;

  uint16_t current_universe = 0;
  printf(BEGIN_BORDER_STRING);
  printf("Enter the current universe number:\n");
  scanf("%hu", &current_universe);

  ListeningUniverse* listener = find_listener_on_universe(current_universe);
  if (listener)
  {
    uint16_t new_universe = 0;
    printf("Enter the new universe number:\n");
    scanf("%hu", &new_universe);

    result = update_listener_universe(listener, new_universe, callbacks);
    printf("Result: %s\n", etcpal_strerror(result));
  }
  else
  {
    printf("There are no receivers currently listening to universe %hu.\n", current_universe);
  }

  printf(END_BORDER_STRING);

  return result;
}

/**************************************************************************************************
 * sACN Callback Implementations
 *************************************************************************************************/

/*
 * This callback is called when a sACN data packet is received on a universe. In a real application
 * this data would be acted upon somehow. We just update some stats about the source of the data,
 * and store the first few slots.
 */
static void handle_universe_data(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                 const SacnRemoteSource* source_info, const SacnRecvUniverseData* universe_data,
                                 void* context)
{
  ETCPAL_UNUSED_ARG(receiver_handle);
  ETCPAL_UNUSED_ARG(source_addr);

  if (etcpal_mutex_lock(&mutex))
  {
    ListeningUniverse* listener = (ListeningUniverse*)context;
    assert(listener);

    // Get the source state to update.
    SourceData* source = find_source(listener, &source_info->cid);
    if (source == NULL)
    {
      // See if there's room for new source data
      source = find_source_hole(listener);
      if (source)
      {
        source->cid = source_info->cid;
        strcpy(source->name, source_info->name);
        source->num_updates = 0;
        source->update_start_time_ms = etcpal_getms();
        source->valid = true;
        ++listener->num_sources;
      }
      else
      {
        printf("No room to track new source on universe %u\n", universe_data->universe_id);
      }
    }

    if (source)
    {
      ++source->num_updates;
      source->priority = universe_data->priority;
      size_t values_len = (universe_data->slot_range.address_count < NUM_SLOTS_DISPLAYED)
                              ? universe_data->slot_range.address_count
                              : NUM_SLOTS_DISPLAYED;
      memcpy(source->last_update, universe_data->values, values_len);
      memset(source->last_update + values_len, 0, NUM_SLOTS_DISPLAYED - values_len);
    }

    etcpal_mutex_unlock(&mutex);
  }
}

/*
 * This callback is called when one or more sources are lost on a universe. We remove them from our
 * tracking. In a real application this would be where any hold-last-look, etc. logic is
 * triggered for a universe.
 */
static void handle_sources_lost(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                                size_t num_lost_sources, void* context)
{
  ETCPAL_UNUSED_ARG(handle);

  if (etcpal_mutex_lock(&mutex))
  {
    ListeningUniverse* listener = (ListeningUniverse*)context;
    assert(listener);

    printf("Universe %u lost the following source(s):\n", universe);
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
 * The sampling period has ended. Just log a message here.
 */
void handle_sampling_period_ended(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(context);

  printf("Sampling period ended on universe %u.\n", universe);
}

/*
 * The sampling period has begun. Just log a message here.
 */
void handle_sampling_period_started(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(context);

  printf("Sampling period started on universe %u.\n", universe);
}

/*
 * If sACN is compiled with SACN_ETC_PRIORITY_EXTENSION=1, this callback is called if a source was
 * sending per-channel priority packets and stopped sending them, but is still sending NULL start
 * code data.
 */
static void handle_source_pap_lost(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source,
                                   void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(context);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&source->cid, cid_str);
  printf("Per-channel priority lost for source '%s' (%s)\n", source->name, cid_str);
}

static void handle_source_limit_exceeded(sacn_receiver_t handle, uint16_t universe, void* context)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(context);

  printf("Source limit exceeded on universe %u\n", universe);
}

// clang-format off
static const SacnReceiverCallbacks kSacnCallbacks = {
  handle_universe_data,
  handle_sources_lost,
  handle_sampling_period_started,
  handle_sampling_period_ended,
  handle_source_pap_lost,
  handle_source_limit_exceeded,
  NULL
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
  log_params.action = ETCPAL_LOG_CREATE_HUMAN_READABLE;
  log_params.log_fn = log_callback;
  log_params.time_fn = NULL;
  log_params.log_mask = ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG);

  etcpal_error_t sacn_init_result = sacn_init(&log_params, NULL);
  if (sacn_init_result != kEtcPalErrOk)
  {
    printf("sACN initialization failed with error: '%s'\n", etcpal_strerror(sacn_init_result));
    return 1;
  }

  // Initialize the listener array by making each handle invalid since there are no listeners to start.
  invalidate_listeners();

  // Handle Ctrl+C gracefully and shut down in compatible consoles
  install_keyboard_interrupt_handler(handle_keyboard_interrupt);

  // Handle user input until we are told to stop
  bool print_prompt = true;
  while (keep_running)
  {
    if (print_prompt)
    {
      printf("Enter input (enter h for help):\n");
    }
    else
    {
      print_prompt = true;  // Print it next time by default.
    }

    etcpal_error_t console_result = kEtcPalErrOk;

    int ch = getchar();
    switch (ch)
    {
      case 'h':
        console_print_help();
        break;
      case 'p':
        console_print_universe_updates();
        break;
      case 'a':
        console_result = console_add_listening_universe(&kSacnCallbacks);
        break;
      case 'r':
        console_result = console_remove_listening_universe();
        break;
      case 'c':
        console_result = console_change_listening_universe(&kSacnCallbacks);
        break;
      case '\n':
        print_prompt = false;  // Otherwise the prompt is printed twice.
        break;
      case EOF:  // Ctrl-C
        keep_running = false;
        break;
      default:
        printf("Invalid input.\n");
    }

    if (console_result != kEtcPalErrOk)
    {
      printf("A critical error has occurred. Press ctrl-c to end this program.\n");
      while (getchar() != EOF)
        ;
      keep_running = false;  // Shut down on error.
    }
  }

  printf("Shutting down sACN...\n");

  for (ListeningUniverse* listener = listeners; listener < listeners + MAX_LISTENERS; ++listener)
  {
    if (listener->receiver_handle != SACN_RECEIVER_INVALID)
    {
      destroy_listener(listener);
    }
  }

  sacn_deinit();
  etcpal_mutex_destroy(&mutex);
  return 0;
}
