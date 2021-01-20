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
#include "sacn/private/pdu.h"
#include "sacn/private/sockets.h"
#include "sacn/private/source.h"
#include "sacn/private/util.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "etcpal/rbtree.h"
#include "etcpal/timer.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

// Suppress strncpy() warning on Windows/MSVC.
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

/****************************** Private macros *******************************/

#define SOURCE_THREAD_INTERVAL 23
#define UNIVERSE_DISCOVERY_INTERVAL 10000
#define SOURCE_ENABLED                                                                                   \
  ((!SACN_DYNAMIC_MEM && (SACN_SOURCE_MAX_SOURCES > 0) && (SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE > 0)) || \
   SACN_DYNAMIC_MEM)
#define UNICAST_ENABLED ((!SACN_DYNAMIC_MEM && (SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE > 0)) || SACN_DYNAMIC_MEM)
#define IS_PART_OF_UNIVERSE_DISCOVERY(universe) (universe->has_null_data && !universe->send_unicast_only)

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

#if SACN_DYNAMIC_MEM
#define ALLOC_SOURCE_STATE() malloc(sizeof(SourceState))
#define FREE_SOURCE_STATE(ptr)                                                         \
  do                                                                                   \
  {                                                                                    \
    etcpal_rbtree_clear_with_cb(&((SourceState*)ptr)->universes, free_universes_node); \
    etcpal_rbtree_clear_with_cb(&((SourceState*)ptr)->netints, free_netints_node);     \
    free(ptr);                                                                         \
  } while (0)
#define ALLOC_UNIVERSE_STATE() malloc(sizeof(UniverseState))
#define FREE_UNIVERSE_STATE(ptr)                                                                 \
  do                                                                                             \
  {                                                                                              \
    etcpal_rbtree_clear_with_cb(&((UniverseState*)ptr)->unicast_dests, free_unicast_dests_node); \
    if (((UniverseState*)ptr)->netints)                                                          \
      free(((UniverseState*)ptr)->netints);                                                      \
    free(ptr);                                                                                   \
  } while (0)
#define ALLOC_SOURCE_NETINT() malloc(sizeof(NetintState))
#define FREE_SOURCE_NETINT(ptr) free(ptr)
#define ALLOC_UNICAST_DESTINATION() malloc(sizeof(UnicastDestination))
#define FREE_UNICAST_DESTINATION(ptr) free(ptr)
#define ALLOC_SOURCE_RB_NODE() malloc(sizeof(EtcPalRbNode))
#define FREE_SOURCE_RB_NODE(ptr) free(ptr)
#elif SOURCE_ENABLED
#define ALLOC_SOURCE_STATE() etcpal_mempool_alloc(sacnsource_source_states)
#define FREE_SOURCE_STATE(ptr)                                                         \
  do                                                                                   \
  {                                                                                    \
    etcpal_rbtree_clear_with_cb(&((SourceState*)ptr)->universes, free_universes_node); \
    etcpal_rbtree_clear_with_cb(&((SourceState*)ptr)->netints, free_netints_node);     \
    etcpal_mempool_free(sacnsource_source_states, ptr);                                \
  } while (0)
#define ALLOC_UNIVERSE_STATE() etcpal_mempool_alloc(sacnsource_universe_states)
#define FREE_UNIVERSE_STATE(ptr)                                                                 \
  do                                                                                             \
  {                                                                                              \
    etcpal_rbtree_clear_with_cb(&((UniverseState*)ptr)->unicast_dests, free_unicast_dests_node); \
    etcpal_mempool_free(sacnsource_universe_states, ptr);                                        \
  } while (0)
#define ALLOC_SOURCE_NETINT() etcpal_mempool_alloc(sacnsource_netints)
#define FREE_SOURCE_NETINT(ptr) etcpal_mempool_free(sacnsource_netints, ptr)
#if UNICAST_ENABLED
#define ALLOC_UNICAST_DESTINATION() etcpal_mempool_alloc(sacnsource_unicast_dests)
#define FREE_UNICAST_DESTINATION(ptr) etcpal_mempool_free(sacnsource_unicast_dests, ptr)
#else
#define ALLOC_UNICAST_DESTINATION() NULL
#define FREE_UNICAST_DESTINATION(ptr)
#endif
#define ALLOC_SOURCE_RB_NODE() etcpal_mempool_alloc(sacnsource_rb_nodes)
#define FREE_SOURCE_RB_NODE(ptr) etcpal_mempool_free(sacnsource_rb_nodes, ptr)
#else
#define ALLOC_SOURCE_STATE() NULL
#define FREE_SOURCE_STATE(ptr)
#define ALLOC_UNIVERSE_STATE() NULL
#define FREE_UNIVERSE_STATE(ptr)
#define ALLOC_SOURCE_NETINT() NULL
#define FREE_SOURCE_NETINT(ptr)
#define ALLOC_UNICAST_DESTINATION() NULL
#define FREE_UNICAST_DESTINATION(ptr)
#define ALLOC_SOURCE_RB_NODE() NULL
#define FREE_SOURCE_RB_NODE(ptr)
#endif

/****************************** Private types ********************************/

typedef struct SourceState
{
  sacn_source_t handle;  // This must be the first struct member.

  EtcPalUuid cid;
  char name[SACN_SOURCE_NAME_MAX_LEN];

  bool terminating;  // If in the process of terminating all universes and removing this source.

  EtcPalRbTree universes;
  size_t num_active_universes;  // Number of universes to include in universe discovery packets.
  bool universe_discovery_updated;
  EtcPalTimer universe_discovery_timer;
  bool process_manually;
  sacn_ip_support_t ip_supported;
  int keep_alive_interval;
  size_t universe_count_max;

  EtcPalRbTree netints;  // Provides a way to look up netints being used by any universe of this source.

  uint8_t universe_discovery_send_buf[SACN_MTU];

} SourceState;

typedef struct UniverseState
{
  uint16_t universe_id;  // This must be the first struct member.

  bool terminating;
  int num_terminations_sent;

  uint8_t priority;
  uint16_t universe;
  uint16_t sync_universe;
  bool send_preview;
  uint8_t seq_num;

  // Start code 0x00 state
  int null_packets_sent_before_suppression;
  EtcPalTimer null_keep_alive_timer;
  uint8_t null_send_buf[SACN_MTU];
  bool has_null_data;

#if SACN_ETC_PRIORITY_EXTENSION
  // Start code 0xDD state
  int pap_packets_sent_before_suppression;
  EtcPalTimer pap_keep_alive_timer;
  uint8_t pap_send_buf[SACN_MTU];
  bool has_pap_data;
#endif

  EtcPalRbTree unicast_dests;
  bool send_unicast_only;

#if SACN_DYNAMIC_MEM
  EtcPalMcastNetintId* netints;
#else
  EtcPalMcastNetintId netints[SACN_MAX_NETINTS];
#endif
  size_t num_netints;  // Number of elements in the netints array.
} UniverseState;

typedef struct NetintState
{
  EtcPalMcastNetintId id;  // This must be the first struct member.
  size_t num_refs;         // Number of universes using this netint.
} NetintState;

typedef struct UnicastDestination
{
  EtcPalIpAddr dest_addr;  // This must be the first struct member.
  bool ready_for_processing;
  bool terminating;
  int num_terminations_sent;
} UnicastDestination;

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM && SOURCE_ENABLED
ETCPAL_MEMPOOL_DEFINE(sacnsource_source_states, SourceState, SACN_SOURCE_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnsource_universe_states, UniverseState,
                      (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE));
ETCPAL_MEMPOOL_DEFINE(sacnsource_netints, NetintState, (SACN_SOURCE_MAX_SOURCES * SACN_MAX_NETINTS));
#if UNICAST_ENABLED
ETCPAL_MEMPOOL_DEFINE(sacnsource_unicast_dests, UnicastDestination,
                      (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE *
                       SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE));
#endif
ETCPAL_MEMPOOL_DEFINE(sacnsource_rb_nodes, EtcPalRbNode,
                      SACN_SOURCE_MAX_SOURCES + (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE) +
                          (SACN_SOURCE_MAX_SOURCES * SACN_MAX_NETINTS) +
                          (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE *
                           SACN_MAX_UNICAST_DESTINATIONS_PER_UNIVERSE));
#endif

static IntHandleManager source_handle_mgr;
static EtcPalRbTree sources;
static bool sources_initialized = false;
static bool shutting_down = false;
static etcpal_thread_t source_thread_handle;
static bool thread_initialized = false;

/*********************** Private function prototypes *************************/

static int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int universe_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int netint_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int unicast_dests_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);

static etcpal_error_t lookup_state(sacn_source_t source, uint16_t universe, SourceState** source_state,
                                   UniverseState** universe_state);
static etcpal_error_t lookup_unicast_dest(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* addr,
                                          UnicastDestination** unicast_dest);

static EtcPalRbNode* source_rb_node_alloc_func(void);
static void source_rb_node_dealloc_func(EtcPalRbNode* node);
static void free_universes_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_netints_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_unicast_dests_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_sources_node(const EtcPalRbTree* self, EtcPalRbNode* node);

static bool source_handle_in_use(int handle_val, void* cookie);

static etcpal_error_t start_tick_thread();
static void stop_tick_thread();

static void source_thread_function(void* arg);

static int process_internal(bool process_manual);
static int process_sources(bool process_manual);
static void process_universe_discovery(SourceState* source);
static void process_universes(SourceState* source);
static void process_unicast_dests(SourceState* source, UniverseState* universe);
static void process_universe_null_pap_transmission(SourceState* source, UniverseState* universe);
static void remove_unicast_dest(UniverseState* universe, UnicastDestination** dest, EtcPalRbIter* unicast_iter);
static void remove_universe_state(SourceState* source, UniverseState** universe, EtcPalRbIter* universe_iter);
static void remove_source_state(SourceState** source, EtcPalRbIter* source_iter);
static void send_data_multicast_ipv4_ipv6(const SourceState* source, UniverseState* universe, const uint8_t* send_buf);
static void send_data_unicast_ipv4_ipv6(const SourceState* source, UniverseState* universe, const uint8_t* send_buf);
static void increment_sequence_number(UniverseState* universe);
static void send_null_data_multicast(const SourceState* source, UniverseState* universe);
static void send_null_data_unicast(const SourceState* source, UniverseState* universe);
static void process_null_sent(UniverseState* universe);
#if SACN_ETC_PRIORITY_EXTENSION
static void send_pap_data_multicast(const SourceState* source, UniverseState* universe);
static void send_pap_data_unicast(const SourceState* source, UniverseState* universe);
static void process_pap_sent(UniverseState* universe);
#endif
static void send_termination_multicast(const SourceState* source, UniverseState* universe);
static void send_termination_unicast(const SourceState* source, UniverseState* universe, UnicastDestination* dest);
static void send_universe_discovery(SourceState* source);
static void send_data_unicast(etcpal_iptype_t ip_type, const uint8_t* send_buf, EtcPalRbTree* dests);
static void send_data_to_single_unicast_dest(etcpal_iptype_t ip_type, const uint8_t* send_buf,
                                             const UnicastDestination* dest);
static int pack_universe_discovery_page(SourceState* source, EtcPalRbIter* universe_iter, uint8_t page_number);
static void init_send_buf(uint8_t* send_buf, uint8_t start_code, const EtcPalUuid* source_cid, const char* source_name,
                          uint8_t priority, uint16_t universe, uint16_t sync_universe, bool send_preview);
static void update_data(uint8_t* send_buf, const uint8_t* new_data, uint16_t new_data_size, bool force_sync);
static void update_levels(SourceState* source_state, UniverseState* universe_state, const uint8_t* new_levels,
                          size_t new_levels_size, bool force_sync);
#if SACN_ETC_PRIORITY_EXTENSION
static void update_paps(SourceState* source_state, UniverseState* universe_state, const uint8_t* new_priorities,
                        size_t new_priorities_size, bool force_sync);
#endif
static void update_levels_and_or_paps(SourceState* source, UniverseState* universe, const uint8_t* new_values,
                                      size_t new_values_size, const uint8_t* new_priorities,
                                      size_t new_priorities_size, bool force_sync);
static void set_unicast_dests_ready(UniverseState* universe_state);
static void set_source_terminating(SourceState* source);
static void set_universe_terminating(UniverseState* universe);
static void set_unicast_dest_terminating(UnicastDestination* dest);
static void reset_transmission_suppression(const SourceState* source, UniverseState* universe, bool reset_null,
                                           bool reset_pap);
static void set_source_name(SourceState* source, const char* new_name);
static void set_universe_priority(const SourceState* source, UniverseState* universe, uint8_t priority);
static void set_preview_flag(const SourceState* source, UniverseState* universe, bool preview);

/*************************** Function definitions ****************************/

/* Initialize the sACN Source module. Internal function called from sacn_init(). */
etcpal_error_t sacn_source_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if SOURCE_ENABLED
#if !SACN_DYNAMIC_MEM
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_source_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_universe_states);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_netints);
  if (res == kEtcPalErrOk)
    res = etcpal_mempool_init(sacnsource_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&sources, source_state_lookup_compare_func, source_rb_node_alloc_func,
                       source_rb_node_dealloc_func);
    sources_initialized = true;

    init_int_handle_manager(&source_handle_mgr, source_handle_in_use, NULL);
  }

  if (res != kEtcPalErrOk)
    sacn_source_deinit();  // Clean up
#endif                     // SOURCE_ENABLED

  return res;
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

  if (sacn_lock())
  {
    if (sources_initialized)
    {
      etcpal_rbtree_clear_with_cb(&sources, free_sources_node);
      sources_initialized = false;
    }

    sacn_unlock();
  }
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
#if !SOURCE_ENABLED
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
    SourceState* source = NULL;
    if (result == kEtcPalErrOk)
    {
      source = ALLOC_SOURCE_STATE();

      if (!source)
        result = kEtcPalErrNoMem;
    }

    // Initialize the source's state and add it to the sources tree.
    if (result == kEtcPalErrOk)
    {
      // Initialize the universe discovery send buffer.
      memset(source->universe_discovery_send_buf, 0, SACN_MTU);
      size_t written = 0;
      written += pack_sacn_root_layer(source->universe_discovery_send_buf, SACN_UNIVERSE_DISCOVERY_HEADER_SIZE, true,
                                      &config->cid);
      written +=
          pack_sacn_universe_discovery_framing_layer(&source->universe_discovery_send_buf[written], 0, config->name);
      written += pack_sacn_universe_discovery_layer_header(&source->universe_discovery_send_buf[written], 0, 0, 0);

      // Initialize everything else.
      source->handle = get_next_int_handle(&source_handle_mgr, -1);
      source->cid = config->cid;
      memset(source->name, 0, SACN_SOURCE_NAME_MAX_LEN);
      memcpy(source->name, config->name, strlen(config->name));
      source->terminating = false;
      etcpal_rbtree_init(&source->universes, universe_state_lookup_compare_func, source_rb_node_alloc_func,
                         source_rb_node_dealloc_func);
      source->num_active_universes = 0;
      source->universe_discovery_updated = true;
      etcpal_timer_start(&source->universe_discovery_timer, UNIVERSE_DISCOVERY_INTERVAL);
      source->process_manually = config->manually_process_source;
      source->ip_supported = config->ip_supported;
      source->keep_alive_interval = config->keep_alive_interval;
      source->universe_count_max = config->universe_count_max;
      etcpal_rbtree_init(&source->netints, netint_state_lookup_compare_func, source_rb_node_alloc_func,
                         source_rb_node_dealloc_func);

      // Add the state to the sources tree.
      etcpal_error_t insert_result = etcpal_rbtree_insert(&sources, source);

      // Clean up on failure.
      if (insert_result != kEtcPalErrOk)
      {
        FREE_SOURCE_STATE(source);

        if (insert_result == kEtcPalErrNoMem)
          result = kEtcPalErrNoMem;
        else
          result = kEtcPalErrSys;
      }
    }

    // Initialize the handle on success.
    if (result == kEtcPalErrOk)
      *handle = source->handle;

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
#if !SOURCE_ENABLED
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
    SourceState* source = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_state(handle, 0, &source, NULL);

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
#if SOURCE_ENABLED
  if (sacn_initialized() && (handle != SACN_SOURCE_INVALID) && sacn_lock())
  {
    // Try to find the source's state.
    SourceState* source = etcpal_rbtree_find(&sources, &handle);

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
#if !SOURCE_ENABLED
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
    SourceState* source = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_state(handle, 0, &source, NULL);

    // Confirm that the universe wasn't already added.
    if (result == kEtcPalErrOk)
    {
      UniverseState* tmp;
      if (lookup_state(handle, config->universe, &source, &tmp) != kEtcPalErrNotFound)
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
    UniverseState* universe = NULL;
    if (result == kEtcPalErrOk)
    {
      universe = ALLOC_UNIVERSE_STATE();

      if (!universe)
        result = kEtcPalErrNoMem;
    }

    // Initialize the universe's state.
    if (result == kEtcPalErrOk)
    {
      universe->universe_id = config->universe;

      universe->terminating = false;
      universe->num_terminations_sent = 0;

      universe->priority = config->priority;
      universe->universe = config->universe;
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

      etcpal_rbtree_init(&universe->unicast_dests, unicast_dests_lookup_compare_func, source_rb_node_alloc_func,
                         source_rb_node_dealloc_func);
      for (size_t i = 0; (result == kEtcPalErrOk) && (i < config->num_unicast_destinations); ++i)
      {
        UnicastDestination* dest = ALLOC_UNICAST_DESTINATION();
        if (dest)
        {
          dest->dest_addr = config->unicast_destinations[i];
          dest->ready_for_processing = false;  // Calling an Update Values function sets this to true.
          dest->terminating = false;
          dest->num_terminations_sent = 0;

          etcpal_error_t insert_result = etcpal_rbtree_insert(&universe->unicast_dests, dest);

          if (insert_result == kEtcPalErrNoMem)
            result = kEtcPalErrNoMem;
          else if (insert_result == kEtcPalErrExists)
            result = kEtcPalErrOk;  // Duplicates are automatically filtered and not a failure condition.
          else if (insert_result == kEtcPalErrOk)
            result = kEtcPalErrOk;
          else
            result = kEtcPalErrSys;

          if (insert_result != kEtcPalErrOk)
            FREE_UNICAST_DESTINATION(dest);
        }
        else
        {
          result = kEtcPalErrNoMem;
        }
      }
    }

    if (result == kEtcPalErrOk)
      result = sacn_initialize_internal_netints(&universe->netints, &universe->num_netints, netints, num_netints);

    // Add the universe's state to the source's universes tree.
    if (result == kEtcPalErrOk)
    {
      etcpal_error_t insert_result = etcpal_rbtree_insert(&source->universes, universe);

      if (insert_result == kEtcPalErrNoMem)
        result = kEtcPalErrNoMem;
      else if (insert_result != kEtcPalErrOk)
        result = kEtcPalErrSys;
    }

    // Update the source's netint tracking.
    for (size_t i = 0; (result == kEtcPalErrOk) && (i < universe->num_netints); ++i)
    {
      NetintState* netint = etcpal_rbtree_find(&source->netints, &universe->netints[i]);

      if (netint)
      {
        // Update existing netint by incrementing the ref counter.
        ++netint->num_refs;
      }
      else
      {
        // Add a new netint with ref counter = 1.
        netint = ALLOC_SOURCE_NETINT();

        if (netint)
        {
          netint->id = universe->netints[i];
          netint->num_refs = 1;

          etcpal_error_t insert_result = etcpal_rbtree_insert(&source->netints, netint);

          if (insert_result == kEtcPalErrNoMem)
            result = kEtcPalErrNoMem;
          else if (insert_result != kEtcPalErrOk)
            result = kEtcPalErrSys;
        }
        else
        {
          result = kEtcPalErrNoMem;
        }

        // Clean up netint on failure.
        if ((result != kEtcPalErrOk) && netint)
          FREE_SOURCE_NETINT(netint);
      }
    }

    // Clean up universe on failure.
    if ((result != kEtcPalErrOk) && universe)
      FREE_UNIVERSE_STATE(universe);

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
#if SOURCE_ENABLED
  if (sacn_lock())
  {
    UniverseState* universe_state = NULL;
    lookup_state(handle, universe, NULL, &universe_state);

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

#if SOURCE_ENABLED
  if (sacn_lock())
  {
    // Look up source state
    SourceState* source = NULL;
    if (lookup_state(handle, 0, &source, NULL) == kEtcPalErrOk)
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
 * Adds a unicast destination for a source's universe.
 * After this call completes, the applicaton must call a variant of sacn_source_update_values() to mark it ready for
 * processing.
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
#if !SOURCE_ENABLED
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
    // Look up the universe state
    UniverseState* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_state(handle, universe, NULL, &universe_state);

    // Allocate the unicast destination
    UnicastDestination* unicast_dest = NULL;
    if (result == kEtcPalErrOk)
    {
      unicast_dest = ALLOC_UNICAST_DESTINATION();
      if (!unicast_dest)
        result = kEtcPalErrNoMem;
    }

    // Add unicast destination
    if (result == kEtcPalErrOk)
    {
      unicast_dest->dest_addr = *dest;
      unicast_dest->ready_for_processing = false;  // Calling an Update Values function sets this to true.
      unicast_dest->terminating = false;
      unicast_dest->num_terminations_sent = 0;

      etcpal_error_t insert_result = etcpal_rbtree_insert(&universe_state->unicast_dests, unicast_dest);

      if (insert_result == kEtcPalErrExists)
        result = kEtcPalErrExists;
      else if (insert_result == kEtcPalErrNoMem)
        result = kEtcPalErrNoMem;
      else if (insert_result != kEtcPalErrOk)
        result = kEtcPalErrSys;

      if (insert_result != kEtcPalErrOk)
        FREE_UNICAST_DESTINATION(unicast_dest);
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
#if SOURCE_ENABLED
  // Validate & lock
  if (dest && sacn_lock())
  {
    // Look up unicast destination
    UnicastDestination* unicast_dest = NULL;
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

#if SOURCE_ENABLED
  if (sacn_lock())
  {
    // Look up universe state
    UniverseState* universe_state = NULL;
    if (lookup_state(handle, universe, NULL, &universe_state) == kEtcPalErrOk)
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
#if !SOURCE_ENABLED
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
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_state(handle, universe, &source_state, &universe_state);

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
#if !SOURCE_ENABLED
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
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_state(handle, universe, &source_state, &universe_state);

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
#if !SOURCE_ENABLED
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
#if !SOURCE_ENABLED
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
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    if (result == kEtcPalErrOk)
      result = lookup_state(handle, universe, &source_state, &universe_state);

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
#if !SOURCE_ENABLED
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
#if SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    lookup_state(handle, universe, &source_state, &universe_state);
    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, NULL, 0, false);
    sacn_unlock();
  }
#endif  // SOURCE_ENABLED
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
#if SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && (new_priorities_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    lookup_state(handle, universe, &source_state, &universe_state);

    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, new_priorities,
                              new_priorities_size, false);

    // Stop using PAPs if new_priorities is NULL
    if (!new_priorities)
      universe_state->has_pap_data = false;

    sacn_unlock();
  }
#endif  // SOURCE_ENABLED
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
#if SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    lookup_state(handle, universe, &source_state, &universe_state);
    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, NULL, 0, true);
    sacn_unlock();
  }
#endif  // SOURCE_ENABLED
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
#if SOURCE_ENABLED
  if (new_values && (new_values_size <= DMX_ADDRESS_COUNT) && (new_priorities_size <= DMX_ADDRESS_COUNT) && sacn_lock())
  {
    SourceState* source_state = NULL;
    UniverseState* universe_state = NULL;
    lookup_state(handle, universe, &source_state, &universe_state);

    update_levels_and_or_paps(source_state, universe_state, new_values, new_values_size, new_priorities,
                              new_priorities_size, true);

    // Stop using PAPs if new_priorities is NULL
    if (!new_priorities)
      universe_state->has_pap_data = false;

    
    sacn_unlock();
  }
#endif  // SOURCE_ENABLED
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
#if !SOURCE_ENABLED
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
#if !SOURCE_ENABLED
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

#if SOURCE_ENABLED
  if (sacn_lock())
  {
    // Look up universe state
    UniverseState* universe_state = NULL;
    if (lookup_state(handle, universe, NULL, &universe_state) == kEtcPalErrOk)
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

static int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const SourceState* a = (const SourceState*)value_a;
  const SourceState* b = (const SourceState*)value_b;

  return (a->handle > b->handle) - (a->handle < b->handle);  // Just compare the handles.
}

int universe_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const UniverseState* a = (const UniverseState*)value_a;
  const UniverseState* b = (const UniverseState*)value_b;

  return (a->universe_id > b->universe_id) - (a->universe_id < b->universe_id);  // Just compare the IDs.
}

int netint_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const NetintState* a = (const NetintState*)value_a;
  const NetintState* b = (const NetintState*)value_b;

  return ((a->id.index > b->id.index) || ((a->id.index == b->id.index) && (a->id.ip_type > b->id.ip_type))) -
         ((a->id.index < b->id.index) || ((a->id.index == b->id.index) && (a->id.ip_type < b->id.ip_type)));
}

int unicast_dests_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const EtcPalIpAddr* a = (const EtcPalIpAddr*)value_a;
  const EtcPalIpAddr* b = (const EtcPalIpAddr*)value_b;

  bool greater_than = a->type > b->type;
  bool less_than = a->type < b->type;

  if (a->type == b->type)
  {
    if (a->type == kEtcPalIpTypeV4)
    {
      greater_than = a->addr.v4 > b->addr.v4;
      less_than = a->addr.v4 < b->addr.v4;
    }
    else if (a->type == kEtcPalIpTypeV6)
    {
      int cmp = memcmp(a->addr.v6.addr_buf, b->addr.v6.addr_buf, ETCPAL_IPV6_BYTES);
      greater_than = (cmp > 0);
      less_than = (cmp < 0);

      if (cmp == 0)
      {
        greater_than = a->addr.v6.scope_id > b->addr.v6.scope_id;
        less_than = a->addr.v6.scope_id < b->addr.v6.scope_id;
      }
    }
  }

  return (int)greater_than - (int)less_than;
}

// Needs lock
etcpal_error_t lookup_state(sacn_source_t source, uint16_t universe, SourceState** source_state,
                            UniverseState** universe_state)
{
  etcpal_error_t result = kEtcPalErrOk;

  SourceState* my_source_state = NULL;
  UniverseState* my_universe_state = NULL;

  // Look up the source state.
  my_source_state = etcpal_rbtree_find(&sources, &source);

  if (!my_source_state)
    result = kEtcPalErrNotFound;

  // Look up the universe state.
  if ((result == kEtcPalErrOk) && universe_state)
  {
    my_universe_state = etcpal_rbtree_find(&my_source_state->universes, &universe);

    if (!my_universe_state)
      result = kEtcPalErrNotFound;
  }

  if (result == kEtcPalErrOk)
  {
    if (source_state)
      *source_state = my_source_state;
    if (universe_state)
      *universe_state = my_universe_state;
  }

  return result;
}

// Needs lock
etcpal_error_t lookup_unicast_dest(sacn_source_t source, uint16_t universe, const EtcPalIpAddr* addr,
                                   UnicastDestination** unicast_dest)
{
  // Look up universe
  UniverseState* universe_state = NULL;
  etcpal_error_t result = lookup_state(source, universe, NULL, &universe_state);

  // Validate
  if ((result == kEtcPalErrOk) && (!addr || !unicast_dest))
    result = kEtcPalErrSys;

  // Look up unicast destination
  UnicastDestination* found = NULL;
  if (result == kEtcPalErrOk)
  {
    found = etcpal_rbtree_find(&universe_state->unicast_dests, addr);

    if (!found)
      result = kEtcPalErrNotFound;
  }

  // Pass back to application
  if (result == kEtcPalErrOk)
  {
    *unicast_dest = found;
  }

  return result;
}

static EtcPalRbNode* source_rb_node_alloc_func(void)
{
  return ALLOC_SOURCE_RB_NODE();
}

void source_rb_node_dealloc_func(EtcPalRbNode* node)
{
  FREE_SOURCE_RB_NODE(node);
}

void free_universes_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_UNIVERSE_STATE(node->value);
  FREE_SOURCE_RB_NODE(node);
}

void free_netints_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SOURCE_NETINT(node->value);
  FREE_SOURCE_RB_NODE(node);
}

void free_unicast_dests_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_UNICAST_DESTINATION(node->value);
  FREE_SOURCE_RB_NODE(node);
}

void free_sources_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  FREE_SOURCE_STATE(node->value);
  FREE_SOURCE_RB_NODE(node);
}

bool source_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  return (handle_val == SACN_SOURCE_INVALID) || etcpal_rbtree_find(&sources, &handle_val);
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
  SourceState* source = etcpal_rbiter_first(&source_iter, &sources);
  while (source)
  {
    bool source_incremented = false;

    // If the Source API is shutting down, cause this source to terminate (if thread-based)
    if (!process_manual && shutting_down)
      set_source_terminating(source);

    // If this is the kind of source we want to process (manual vs. thread-based)
    if (source->process_manually == process_manual)
    {
      // Count the sources of the kind being processed by this function
      ++num_sources_tracked;

      // Universe processing
      process_universe_discovery(source);
      process_universes(source);

      // Clean up this source if needed
      if (source->terminating && (etcpal_rbtree_size(&source->universes) == 0))
      {
        remove_source_state(&source, &source_iter);
        source_incremented = true;
      }
    }

    if (!source_incremented)
      source = etcpal_rbiter_next(&source_iter);
  }

  return num_sources_tracked;
}

// Needs lock
void process_universe_discovery(SourceState* source)
{
  // Send another universe discovery packet if it's time
  if (!source->terminating &&
      (source->universe_discovery_updated || etcpal_timer_is_expired(&source->universe_discovery_timer)))
  {
    send_universe_discovery(source);
    source->universe_discovery_updated = false;
    etcpal_timer_reset(&source->universe_discovery_timer);
  }
}

// Needs lock
void process_universes(SourceState* source)
{
  // For each universe of this source
  EtcPalRbIter universe_iter;
  etcpal_rbiter_init(&universe_iter);
  UniverseState* universe = etcpal_rbiter_first(&universe_iter, &source->universes);
  while (universe)
  {
    bool universe_incremented = false;

    // Unicast destination-specific processing
    process_unicast_dests(source, universe);

    // Terminate and clean up this universe if needed
    if (universe->terminating)
    {
      if ((universe->num_terminations_sent < 3) && universe->has_null_data)
        send_termination_multicast(source, universe);

      if (((universe->num_terminations_sent >= 3) && (etcpal_rbtree_size(&universe->unicast_dests) == 0)) ||
          !universe->has_null_data)
      {
        remove_universe_state(source, &universe, &universe_iter);
        universe_incremented = true;
      }
    }
    else
    {
      // Process non-termination transmission of start codes 0x00 and 0xDD
      process_universe_null_pap_transmission(source, universe);
    }

    if (!universe_incremented)
      universe = etcpal_rbiter_next(&universe_iter);
  }
}

// Needs lock
void process_unicast_dests(SourceState* source, UniverseState* universe)
{
  // For each unicast destination of this universe
  EtcPalRbIter unicast_iter;
  etcpal_rbiter_init(&unicast_iter);
  UnicastDestination* dest = etcpal_rbiter_first(&unicast_iter, &universe->unicast_dests);
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
        remove_unicast_dest(universe, &dest, &unicast_iter);
        dest_incremented = true;
      }
    }

    if (!dest_incremented)
      dest = etcpal_rbiter_next(&unicast_iter);
  }
}

// Needs lock
void process_universe_null_pap_transmission(SourceState* source, UniverseState* universe)
{
  // If 0x00 data is ready to send
  if (universe->has_null_data && ((universe->null_packets_sent_before_suppression < 3) ||
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
  if (universe->has_pap_data &&
      ((universe->pap_packets_sent_before_suppression < 3) || etcpal_timer_is_expired(&universe->pap_keep_alive_timer)))
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
void remove_unicast_dest(UniverseState* universe, UnicastDestination** dest, EtcPalRbIter* unicast_iter)
{
  UnicastDestination* dest_to_remove = *dest;
  *dest = etcpal_rbiter_next(unicast_iter);
  etcpal_rbtree_remove(&universe->unicast_dests, dest_to_remove);
  FREE_UNICAST_DESTINATION(dest_to_remove);
}

// Needs lock
void remove_universe_state(SourceState* source, UniverseState** universe, EtcPalRbIter* universe_iter)
{
  UniverseState* universe_to_remove = *universe;

  *universe = etcpal_rbiter_next(universe_iter);

  if (universe_to_remove)
  {
    // Update num_active_universes and universe_discovery_updated if needed
    if (IS_PART_OF_UNIVERSE_DISCOVERY(universe_to_remove))
    {
      --source->num_active_universes;
      source->universe_discovery_updated = true;
    }

    // Update the netints tree
    for (size_t i = 0; i < universe_to_remove->num_netints; ++i)
    {
      NetintState* netint_state = etcpal_rbtree_find(&source->netints, &universe_to_remove->netints[i]);
      if (netint_state)
      {
        if (netint_state->num_refs > 0)
          --netint_state->num_refs;

        if (netint_state->num_refs == 0)
        {
          if (etcpal_rbtree_remove(&source->netints, netint_state) == kEtcPalErrOk)
            FREE_SOURCE_NETINT(netint_state);
        }
      }
    }

    // Now clean up the universe state
    etcpal_rbtree_remove(&source->universes, universe_to_remove);
    FREE_UNIVERSE_STATE(universe_to_remove);
  }
}

// Needs lock
void remove_source_state(SourceState** source, EtcPalRbIter* source_iter)
{
  SourceState* source_to_remove = *source;
  *source = etcpal_rbiter_next(source_iter);
  etcpal_rbtree_remove(&sources, source_to_remove);
  FREE_SOURCE_STATE(source_to_remove);
}

// Needs lock
void send_data_multicast_ipv4_ipv6(const SourceState* source, UniverseState* universe, const uint8_t* send_buf)
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
void send_data_unicast_ipv4_ipv6(const SourceState* source, UniverseState* universe, const uint8_t* send_buf)
{
  // Send unicast on IPv4 and/or IPv6
  if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
    send_data_unicast(kEtcPalIpTypeV4, send_buf, &universe->unicast_dests);
  if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
    send_data_unicast(kEtcPalIpTypeV6, send_buf, &universe->unicast_dests);
}

// Needs lock
void increment_sequence_number(UniverseState* universe)
{
  ++universe->seq_num;
  universe->null_send_buf[SACN_SEQ_OFFSET] = universe->seq_num;
#if SACN_ETC_PRIORITY_EXTENSION
  universe->pap_send_buf[SACN_SEQ_OFFSET] = universe->seq_num;
#endif
}

// Needs lock
void send_null_data_multicast(const SourceState* source, UniverseState* universe)
{
  send_data_multicast_ipv4_ipv6(source, universe, universe->null_send_buf);
}

// Needs lock
void send_null_data_unicast(const SourceState* source, UniverseState* universe)
{
  send_data_unicast_ipv4_ipv6(source, universe, universe->null_send_buf);
}

// Needs lock
void process_null_sent(UniverseState* universe)
{
  increment_sequence_number(universe);

  if (universe->null_packets_sent_before_suppression < 3)
    ++universe->null_packets_sent_before_suppression;
}

#if SACN_ETC_PRIORITY_EXTENSION
// Needs lock
void send_pap_data_multicast(const SourceState* source, UniverseState* universe)
{
  send_data_multicast_ipv4_ipv6(source, universe, universe->pap_send_buf);
}

// Needs lock
void send_pap_data_unicast(const SourceState* source, UniverseState* universe)
{
  send_data_unicast_ipv4_ipv6(source, universe, universe->pap_send_buf);
}

// Needs lock
void process_pap_sent(UniverseState* universe)
{
  increment_sequence_number(universe);

  if (universe->pap_packets_sent_before_suppression < 3)
    ++universe->pap_packets_sent_before_suppression;
}
#endif

// Needs lock
void send_termination_multicast(const SourceState* source, UniverseState* universe)
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
void send_termination_unicast(const SourceState* source, UniverseState* universe, UnicastDestination* dest)
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
void send_universe_discovery(SourceState* source)
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
    for (UnicastDestination* dest = etcpal_rbiter_first(&tree_iter, dests); dest; dest = etcpal_rbiter_next(&tree_iter))
      send_data_to_single_unicast_dest(ip_type, send_buf, dest);
  }
}

// Needs lock
void send_data_to_single_unicast_dest(etcpal_iptype_t ip_type, const uint8_t* send_buf, const UnicastDestination* dest)
{
  // If this destination is ready for processing and matches the IP type, then send to it.
  if (dest && dest->ready_for_processing && (dest->dest_addr.type == ip_type))
    sacn_send_unicast(send_buf, &dest->dest_addr);
}

// Needs lock
int pack_universe_discovery_page(SourceState* source, EtcPalRbIter* universe_iter, uint8_t page_number)
{
  // Initialize packing pointer and universe counter
  uint8_t* pcur = &source->universe_discovery_send_buf[SACN_UNIVERSE_DISCOVERY_HEADER_SIZE];
  int num_universes_packed = 0;

  // Iterate up to 512 universes (sorted)
  for (const UniverseState* universe = (page_number == 0) ? etcpal_rbiter_first(universe_iter, &source->universes)
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
  // Set force sync flag
  SET_FORCE_SYNC_OPT(send_buf, force_sync);

  // Update the size/count fields for the new data size (slot count)
  SET_DATA_SLOT_COUNT(send_buf, new_data_size);

  // Copy data into the send buffer immediately after the start code
  memcpy(&send_buf[SACN_DATA_HEADER_SIZE], new_data, new_data_size);
}

// Needs lock
void update_levels(SourceState* source_state, UniverseState* universe_state, const uint8_t* new_levels,
                   size_t new_levels_size, bool force_sync)
{
  bool was_part_of_discovery = IS_PART_OF_UNIVERSE_DISCOVERY(universe_state);

  update_data(universe_state->null_send_buf, new_levels, (uint16_t)new_levels_size, force_sync);
  universe_state->has_null_data = true;
  reset_transmission_suppression(source_state, universe_state, true, false);

  if (!was_part_of_discovery && IS_PART_OF_UNIVERSE_DISCOVERY(universe_state))
  {
    ++source_state->num_active_universes;
    source_state->universe_discovery_updated = true;
  }
}

#if SACN_ETC_PRIORITY_EXTENSION
// Needs lock
void update_paps(SourceState* source_state, UniverseState* universe_state, const uint8_t* new_priorities,
                 size_t new_priorities_size, bool force_sync)
{
  update_data(universe_state->pap_send_buf, new_priorities, (uint16_t)new_priorities_size, force_sync);
  universe_state->has_pap_data = true;
  reset_transmission_suppression(source_state, universe_state, false, true);
}
#endif

// Needs lock
void update_levels_and_or_paps(SourceState* source, UniverseState* universe, const uint8_t* new_levels,
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
    // Enable new unicast destinations
    set_unicast_dests_ready(universe);
  }
}

// Needs lock
void set_unicast_dests_ready(UniverseState* universe_state)
{
  // For each unicast destination
  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);

  for (UnicastDestination* dest = etcpal_rbiter_first(&tree_iter, &universe_state->unicast_dests); dest;
       dest = etcpal_rbiter_next(&tree_iter))
  {
    // Indicate that this unicast destination is ready for processing.
    dest->ready_for_processing = true;
  }
}

// Needs lock
void set_source_terminating(SourceState* source)
{
  // If the source isn't already terminating
  if (source && !source->terminating)
  {
    // Set the source's terminating flag
    source->terminating = true;

    // Set terminating for each universe of this source
    EtcPalRbIter tree_iter;
    etcpal_rbiter_init(&tree_iter);

    for (UniverseState* universe = etcpal_rbiter_first(&tree_iter, &source->universes); universe;
         universe = etcpal_rbiter_next(&tree_iter))
    {
      set_universe_terminating(universe);
    }
  }
}

// Needs lock
void set_universe_terminating(UniverseState* universe)
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

    for (UnicastDestination* dest = etcpal_rbiter_first(&tree_iter, &universe->unicast_dests); dest;
         dest = etcpal_rbiter_next(&tree_iter))
    {
      set_unicast_dest_terminating(dest);
    }
  }
}

// Needs lock
void set_unicast_dest_terminating(UnicastDestination* dest)
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
void reset_transmission_suppression(const SourceState* source, UniverseState* universe, bool reset_null, bool reset_pap)
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
void set_source_name(SourceState* source, const char* new_name)
{
  // Update the name in the source state and universe discovery buffer
  strncpy(source->name, new_name, SACN_SOURCE_NAME_MAX_LEN);
  strncpy((char*)(&source->universe_discovery_send_buf[SACN_SOURCE_NAME_OFFSET]), new_name, SACN_SOURCE_NAME_MAX_LEN);
  source->universe_discovery_updated = true;  // Cause a new universe discovery packet to go out

  // For each universe:
  EtcPalRbIter tree_iter;
  etcpal_rbiter_init(&tree_iter);
  for (UniverseState* universe = etcpal_rbiter_first(&tree_iter, &source->universes); universe;
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
void set_universe_priority(const SourceState* source, UniverseState* universe, uint8_t priority)
{
  universe->priority = priority;
  universe->null_send_buf[SACN_PRI_OFFSET] = priority;
  universe->pap_send_buf[SACN_PRI_OFFSET] = priority;
  reset_transmission_suppression(source, universe, true, true);
}

// Needs lock
void set_preview_flag(const SourceState* source, UniverseState* universe, bool preview)
{
  universe->send_preview = preview;
  SET_PREVIEW_OPT(universe->null_send_buf, preview);
  SET_PREVIEW_OPT(universe->pap_send_buf, preview);
  reset_transmission_suppression(source, universe, true, true);
}
