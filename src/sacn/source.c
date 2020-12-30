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

/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

#if SACN_DYNAMIC_MEM
#define ALLOC_SOURCE_STATE() malloc(sizeof(SourceState))
#define FREE_SOURCE_STATE(ptr) free(ptr)
#define ALLOC_UNIVERSE_STATE() malloc(sizeof(UniverseState))
#define FREE_UNIVERSE_STATE(ptr) free(ptr)
#define ALLOC_SOURCE_NETINT() malloc(sizeof(NetintState))
#define FREE_SOURCE_NETINT(ptr) free(ptr)
#define ALLOC_SOURCE_RB_NODE() malloc(sizeof(EtcPalRbNode))
#define FREE_SOURCE_RB_NODE(ptr) free(ptr)
#else
#define ALLOC_SOURCE_STATE() etcpal_mempool_alloc(sacnsource_source_states)
#define FREE_SOURCE_STATE(ptr) etcpal_mempool_free(sacnsource_source_states, ptr)
#define ALLOC_UNIVERSE_STATE() etcpal_mempool_alloc(sacnsource_universe_states)
#define FREE_UNIVERSE_STATE(ptr) etcpal_mempool_free(sacnsource_universe_states, ptr)
#define ALLOC_SOURCE_NETINT() etcpal_mempool_alloc(sacnsource_netints)
#define FREE_SOURCE_NETINT(ptr) etcpal_mempool_free(sacnsource_netints, ptr)
#define ALLOC_SOURCE_RB_NODE() etcpal_mempool_alloc(sacnsource_rb_nodes)
#define FREE_SOURCE_RB_NODE(ptr) etcpal_mempool_free(sacnsource_rb_nodes, ptr)
#endif

#define SOURCE_THREAD_INTERVAL 23

/****************************** Private types ********************************/

typedef struct SourceState
{
  sacn_source_t handle;  // This must be the first struct member.

  EtcPalRbTree universes;
  bool universe_list_changed;
  EtcPalTimer universe_discovery_timer;
  bool process_manually;
  sacn_ip_support_t ip_supported;

  EtcPalRbTree netints;  // Provides a way to look up netints being used by any universe of this source.

  uint8_t universe_discovery_send_buf[SACN_MTU];

} SourceState;

typedef struct UniverseState
{
  uint16_t universe_id;  // This must be the first struct member.

  bool terminating;
  int num_terminations_sent;

  // Start code 0x00 state
  int null_packets_sent_before_suppression;
  EtcPalTimer null_keep_alive_timer;
  uint8_t null_send_buf[SACN_MTU];

#if SACN_ETC_PRIORITY_EXTENSION
  // Start code 0xDD state
  int pap_packets_sent_before_suppression;
  EtcPalTimer pap_keep_alive_timer;
  uint8_t pap_send_buf[SACN_MTU];
#endif

#if SACN_DYNAMIC_MEM
  EtcPalIpAddr* unicast_dests;
#else
  EtcPalIpAddr unicast_dests[SACN_MAX_UNICAST_DESTINATIONS];
#endif
  size_t num_unicast_dests;  // Number of elements in the unicast_dests array.
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
  size_t num_refs;  // Number of universes using this netint.
} NetintState;

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacnsource_source_states, SourceState, SACN_SOURCE_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnsource_universe_states, UniverseState,
                      (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE));
ETCPAL_MEMPOOL_DEFINE(sacnsource_netints, NetintState, (SACN_SOURCE_MAX_SOURCES * SACN_MAX_NETINTS));
ETCPAL_MEMPOOL_DEFINE(sacnsource_rb_nodes, EtcPalRbNode,
                      SACN_SOURCE_MAX_SOURCES + (SACN_SOURCE_MAX_SOURCES * SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE) +
                          (SACN_SOURCE_MAX_SOURCES * SACN_MAX_NETINTS));
#endif

static IntHandleManager source_handle_mgr;
static EtcPalRbTree sources;
static etcpal_socket_t ipv4_multicast_sock = ETCPAL_SOCKET_INVALID;
static etcpal_socket_t ipv6_multicast_sock = ETCPAL_SOCKET_INVALID;
static etcpal_socket_t ipv4_unicast_sock = ETCPAL_SOCKET_INVALID;
static etcpal_socket_t ipv6_unicast_sock = ETCPAL_SOCKET_INVALID;
static bool shutting_down = false;
static etcpal_thread_t source_thread_handle;

/*********************** Private function prototypes *************************/

static etcpal_error_t init_multicast_socket(etcpal_socket_t* socket, etcpal_iptype_t ip_type);
static etcpal_error_t init_unicast_socket(etcpal_socket_t* socket, etcpal_iptype_t ip_type);

static int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b);

static EtcPalRbNode* source_rb_node_alloc_func(void);
static void source_rb_node_dealloc_func(EtcPalRbNode* node);
static void free_universes_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_netints_node(const EtcPalRbTree* self, EtcPalRbNode* node);
static void free_sources_node(const EtcPalRbTree* self, EtcPalRbNode* node);

static bool source_handle_in_use(int handle_val, void* cookie);

static etcpal_error_t start_tick_thread();
static void stop_tick_thread();

static void source_thread_function(void* arg);

static int process_internal(bool process_manual);
static void remove_universe_state(const SourceState* source, const UniverseState** universe,
                                  EtcPalRbIter* universe_iter);
static void remove_source_state(const SourceState** source, EtcPalRbIter* source_iter);
static void send_null_data(const SourceState* source, UniverseState* universe);
#if SACN_ETC_PRIORITY_EXTENSION
static void send_pap_data(const SourceState* source, UniverseState* universe);
#endif
static void send_termination(const SourceState* source, UniverseState* universe);
static void send_universe_discovery(SourceState* source);
static void send_data_multicast(uint16_t universe_id, etcpal_iptype_t ip_type, const uint8_t* send_buf,
                                const EtcPalMcastNetintId* netints, size_t num_netints);
static void send_data_unicast(uint16_t universe_id, etcpal_iptype_t ip_type, const uint8_t* send_buf,
                              const EtcPalIpAddr* dests, size_t num_dests);
static int pack_universe_discovery_page(SourceState* source, EtcPalRbIter* universe_iter, int page_number);
static UniverseState* look_up_universe_state(sacn_source_t handle, uint16_t universe);
static void update_data(uint8_t* send_buf, const uint8_t* new_data, size_t new_data_size, bool force_sync);

/*************************** Function definitions ****************************/

/* Initialize the sACN Source module. Internal function called from sacn_init().
   This also starts up the module-provided Tick thread. */
etcpal_error_t sacn_source_init(void)
{
#if ((SACN_SOURCE_MAX_SOURCES <= 0) || (SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE <= 0))
  etcpal_error_t res = kEtcPalErrInvalid;
#else
  etcpal_error_t res = kEtcPalErrOk;
#endif

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
    res = init_multicast_socket(&ipv4_multicast_sock, kEtcPalIpTypeV4);
  if (res == kEtcPalErrOk)
    res = init_multicast_socket(&ipv6_multicast_sock, kEtcPalIpTypeV6);
  if (res == kEtcPalErrOk)
    res = init_unicast_socket(&ipv4_unicast_sock, kEtcPalIpTypeV4);
  if (res == kEtcPalErrOk)
    res = init_unicast_socket(&ipv6_unicast_sock, kEtcPalIpTypeV6);

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&sources, source_state_lookup_compare_func, source_rb_node_alloc_func,
                       source_rb_node_dealloc_func);
    init_int_handle_manager(&source_handle_mgr, source_handle_in_use, NULL);

    res = start_tick_thread();
  }

  return res;
}

void sacn_source_deinit(void)
{
  // Shut down the Tick thread...
  stop_tick_thread();

  if (sacn_lock())
  {
    etcpal_rbtree_clear_with_cb(&sources, free_sources_node);

    if (ipv4_multicast_sock != ETCPAL_SOCKET_INVALID)
      etcpal_close(ipv4_multicast_sock);
    if (ipv6_multicast_sock != ETCPAL_SOCKET_INVALID)
      etcpal_close(ipv6_multicast_sock);
    if (ipv4_unicast_sock != ETCPAL_SOCKET_INVALID)
      etcpal_close(ipv4_unicast_sock);
    if (ipv6_unicast_sock != ETCPAL_SOCKET_INVALID)
      etcpal_close(ipv6_unicast_sock);

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
  ETCPAL_UNUSED_ARG(config);
}

/**
 * @brief Initialize an sACN Source Universe Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_source_universe_config_init(SacnSourceUniverseConfig* config)
{
  ETCPAL_UNUSED_ARG(config);
}

/**
 * @brief Create a new sACN source to send sACN data.
 *
 * This creates the instance of the source, but no data is sent until sacn_source_add_universe() and
 * either sacn_source_update_values() or sacn_source_update_values_and_pap() are called.
 *
 * @param[in] config Configuration parameters for the sACN source to be created.
 * @param[out] handle Filled in on success with a handle to the sACN source.
 * @return #kEtcPalErrOk: Source successfully created.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No room to allocate an additional source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_create(const SacnSourceConfig* config, sacn_source_t* handle)
{
  // If the Tick thread hasn't been started yet, start it if the config isn't manual.

  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(handle);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Change the name of an sACN source.
 *
 * The name is a UTF-8 string representing "a user-assigned name provided by the source of the
 * packet for use in displaying the identity of a source to a user." Only up to
 * #SACN_SOURCE_NAME_MAX_LEN characters will be used.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] new_name New name to use for this universe.
 * @return #kEtcPalErrOk: Name set successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_change_name(sacn_source_t handle, const char* new_name)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_name);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Destroy an sACN source instance.
 *
 * Stops sending all universes for this source. The destruction is queued, and actually occurs
 * on a call to sacn_source_process_all() after an additional three packets have been sent with the
 * "Stream_Terminated" option set. The source will also stop transmitting sACN universe discovery packets.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to sacn_source_add_universe().
 *
 * @param[in] handle Handle to the source to destroy.
 */
void sacn_source_destroy(sacn_source_t handle)
{
  ETCPAL_UNUSED_ARG(handle);
}

/**
 * @brief Add a universe to an sACN source.
 *
 * Adds a universe to a source.
 * After this call completes, the applicaton must call either sacn_source_update_values() or
 * sacn_source_update_values_and_pap() to mark it ready for processing.
 *
 * If the source is not marked as unicast_only, the source will add the universe to its sACN Universe
 * Discovery packets.
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
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or a network interface ID given was not
 * found on the system.
 * @return #kEtcPalErrNoMem: No room to allocate additional universe.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_universe(sacn_source_t handle, const SacnSourceUniverseConfig* config,
                                        SacnMcastInterface* netints, size_t num_netints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(config);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Remove a universe from a source.
 *
 * This queues the source for removal. The destruction actually occurs
 * on a call to sacn_source_process_all() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * The source will also stop transmitting sACN universe discovery packets for that universe.
 *
 * Even though the destruction is queued, after this call the library will no longer use the priorities_buffer
 * or values_buffer you passed in on your call to sacn_source_add_universe().
 *
 * @param[in] handle Handle to the source from which to remove the universe.
 * @param[in] universe Universe to remove.
 */
void sacn_source_remove_universe(sacn_source_t handle, uint16_t universe)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
}

/**
 * @brief Obtain a list of universes this source is transmitting on.
 *
 * @param[in] handle Handle to the source for which to obtain the list of universes.
 * @param[out] universes A pointer to an application-owned array where the universe list will be written.
 * @param[in] universes_size The size of the provided universes array.
 * @return The total number of universes being transmitted by the source. If this is greater than universes_size, then
 * only universes_size universes were written to the universes array. If the source was not found, 0 is returned.
 */
size_t sacn_source_get_universes(sacn_source_t handle, uint16_t* universes, size_t universes_size)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universes);
  ETCPAL_UNUSED_ARG(universes_size);

  return 0;  // TODO
}

/**
 * @brief Add a unicast destination for a source's universe.
 *
 * Adds a unicast destination for a source's universe.
 * After this call completes, the applicaton must call either sacn_source_update_values() or
 * sacn_source_update_values_and_pap() to mark it ready for processing.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL.
 * @return #kEtcPalErrOk: Address added successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source or the universe is not on that source.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_add_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(dest);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Remove a unicast destination on a source's universe.
 *
 * This queues the address for removal. The removal actually occurs
 * on a call to sacn_source_process_all() after an additional three packets have been sent with the
 * "Stream_Terminated" option set.
 *
 * @param[in] handle Handle to the source to change.
 * @param[in] universe Universe to change.
 * @param[in] dest The destination IP.  May not be NULL, and must match the address passed to
 * sacn_source_add_unicast_destination().
 */
void sacn_source_remove_unicast_destination(sacn_source_t handle, uint16_t universe, const EtcPalIpAddr* dest)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(dest);
}

/**
 * @brief Obtain a list of unicast destinations to which this source is transmitting a universe.
 *
 * @param[in] handle Handle to the source that is transmitting on the universe in question.
 * @param[in] universe The universe for which to obtain the list of unicast destinations.
 * @param[out] destinations A pointer to an application-owned array where the unicast destination list will be written.
 * @param[in] destinations_size The size of the provided destinations array.
 * @return The total number of unicast destinations being transmitted by the source for the given universe. If this is
 * greater than destinations_size, then only destinations_size addresses were written to the destinations array. If the
 * source was not found, 0 is returned.
 */
size_t sacn_source_get_unicast_destinations(sacn_source_t handle, uint16_t universe, EtcPalIpAddr* destinations,
                                            size_t destinations_size)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(destinations);
  ETCPAL_UNUSED_ARG(destinations_size);

  return 0;  // TODO
}

/**
 * @brief Change the priority of a universe on a sACN source.
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
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_priority);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Change the send_preview option on a universe of a sACN source.
 *
 * Sets the state of a flag in the outgoing sACN packets that indicates that the data is (from
 * E1.31) "intended for use in visualization or media server preview applications and shall not be
 * used to generate live output."
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
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(new_preview_flag);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Changes the synchronize uinverse for a universe of a sACN source.
 *
 * This will change the synchronization universe used by a sACN universe on the source.
 * If this value is 0, synchronization is turned off for that universe.
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
 * start codes are taken care of by sacn_source_process_all().
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
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(start_code);
  ETCPAL_UNUSED_ARG(buffer);
  ETCPAL_UNUSED_ARG(buflen);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Indicate that a new synchronization packet should be sent on the given synchronization universe.
 *
 * This will cause the transmission of a synchronization packet for the source on the given synchronization universe.
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
  // TODO

  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(sync_universe);
  return kEtcPalErrNotImpl;
}

/**
 * @brief Copies the universe's dmx values into the packet to be sent on the next call to sacn_source_process_all()
 *
 * This function will update the outgoing packet values, and reset the logic that slows down packet transmission due to
 * inactivity.
 *
 * When you don't have per-address priority changes to make, use this function. Otherwise, use
 * sacn_source_update_values_and_pap().
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 */
void sacn_source_update_values(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                               size_t new_values_size)
{
  // Take lock
  sacn_lock();

  // Look up universe state
  UniverseState* universe_state = look_up_universe_state(handle, universe);
  if (universe_state)
  {
    // Update 0x00 values, no force sync
    update_data(universe_state->null_send_buf, new_values, new_values_size, false);
  }

  // Release lock
  sacn_unlock();
}

/**
 * @brief Copies the universe's dmx values and per-address priorities into packets that are sent on the next call to
 * sacn_source_process_all()
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
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities.
 */
void sacn_source_update_values_and_pap(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                       size_t new_values_size, const uint8_t* new_priorities,
                                       size_t new_priorities_size)
{
  // Take lock
  sacn_lock();

  // Look up universe state
  UniverseState* universe_state = look_up_universe_state(handle, universe);
  if (universe_state)
  {
    // Update 0x00 values, no force sync
    update_data(universe_state->null_send_buf, new_values, new_values_size, false);
#if SACN_ETC_PRIORITY_EXTENSION
    // Update 0xDD values, no force sync
    update_data(universe_state->pap_send_buf, new_priorities, new_priorities_size, false);
#endif
  }

  // Release lock
  sacn_unlock();
}

/**
 * @brief Like sacn_source_update_values(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next call to sacn_source_process_all(), and
 * will reset the logic that slows down packet transmission due to inactivity. Additionally, the packet to be sent will
 * have its force_synchronization option flag set.
 *
 * If no synchronization universe is configured, this function acts like a direct call to sacn_source_update_values().
 *
 * TODO: At this time, synchronization is not supported by this library.
 *
 * @param[in] handle Handle to the source to update.
 * @param[in] universe Universe to update.
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 */
void sacn_source_update_values_and_force_sync(sacn_source_t handle, uint16_t universe, const uint8_t* new_values,
                                              size_t new_values_size)
{
  // Take lock
  sacn_lock();

  // Look up universe state
  UniverseState* universe_state = look_up_universe_state(handle, universe);
  if (universe_state)
  {
    // Update 0x00 values, enable force sync
    update_data(universe_state->null_send_buf, new_values, new_values_size, true);
  }

  // Release lock
  sacn_unlock();
}

/**
 * @brief Like sacn_source_update_values_and_pap(), but also sets the force_sync flag on the packet.
 *
 * This function will update the outgoing packet values to be sent on the next call to sacn_source_process_all(), and
 * will reset the logic that slows down packet transmission due to inactivity. Additionally, the final packet to be sent
 * by this call will have its force_synchronization option flag set.
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
 * @param[in] new_values A buffer of dmx values to copy from. This pointer must not be NULL, and only the first 512
 * values will be used.
 * @param[in] new_values_size Size of new_values.
 * @param[in] new_priorities A buffer of per-address priorities to copy from. This may be NULL if you are not using
 * per-address priorities or want to stop using per-address priorities.
 * @param[in] new_priorities_size Size of new_priorities.
 */
void sacn_source_update_values_and_pap_and_force_sync(sacn_source_t handle, uint16_t universe,
                                                      const uint8_t* new_values, size_t new_values_size,
                                                      const uint8_t* new_priorities, size_t new_priorities_size)
{
  // Take lock
  sacn_lock();

  // Look up universe state
  UniverseState* universe_state = look_up_universe_state(handle, universe);
  if (universe_state)
  {
    // Update 0x00 values, enable force sync
    update_data(universe_state->null_send_buf, new_values, new_values_size, true);
#if SACN_ETC_PRIORITY_EXTENSION
    // Update 0xDD values, enable force sync
    update_data(universe_state->pap_send_buf, new_priorities, new_priorities_size, true);
#endif
  }

  // Release lock
  sacn_unlock();
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
 * @return Current number of manual sources tracked by the library. This can be useful on shutdown to
 *         track when destroyed sources have finished sending the terminated packets and actually
 *         been destroyed.
 */
int sacn_source_process_manual(void)
{
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
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid source, or a network interface ID given was not
 * found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_source_reset_networking(sacn_source_t handle, uint16_t universe, SacnMcastInterface* netints,
                                            size_t num_netints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);

  return kEtcPalErrNotImpl;
}

/**
 * @brief Obtain the statuses of a universe's network interfaces.
 *
 * @param[in] handle Handle to the source that includes the universe.
 * @param[in] universe The universe for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the universe. If this is greater than netints_size, then only
 * netints_size addresses were written to the netints array. If the source or universe were not found, 0 is returned.
 */
size_t sacn_source_get_network_interfaces(sacn_source_t handle, uint16_t universe, SacnMcastInterface* netints,
                                          size_t netints_size)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(universe);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(netints_size);

  return 0;  // TODO
}

etcpal_error_t init_multicast_socket(etcpal_socket_t* socket, etcpal_iptype_t ip_type)
{
  etcpal_error_t result = kEtcPalErrOk;

  result = etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, socket);

  int sockopt_ip_level = (ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP);
  if (result == kEtcPalErrOk)
  {
    const int value = 64;
    result = etcpal_setsockopt(*socket, sockopt_ip_level, ETCPAL_IP_MULTICAST_TTL, &value, sizeof value);
  }

  if (result == kEtcPalErrOk)
  {
    int intval = 1;
    result = etcpal_setsockopt(*socket, sockopt_ip_level, ETCPAL_IP_MULTICAST_LOOP, &intval, sizeof intval);
  }

  return result;
}

etcpal_error_t init_unicast_socket(etcpal_socket_t* socket, etcpal_iptype_t ip_type)
{
  return etcpal_socket(ip_type == kEtcPalIpTypeV6 ? ETCPAL_AF_INET6 : ETCPAL_AF_INET, ETCPAL_SOCK_DGRAM, socket);
}

static int source_state_lookup_compare_func(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  const SourceState* a = (const SourceState*)value_a;
  const SourceState* b = (const SourceState*)value_b;

  return (a->handle > b->handle) - (a->handle < b->handle);  // Just compare the handles.
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

void free_sources_node(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  SourceState* source_state = (SourceState*)node->value;

  // Clear the trees within merger state, using callbacks to free memory.
  etcpal_rbtree_clear_with_cb(&source_state->universes, free_universes_node);
  etcpal_rbtree_clear_with_cb(&source_state->netints, free_netints_node);

  // Now free the memory for the merger state and node.
  FREE_SOURCE_STATE(source_state);
  FREE_SOURCE_RB_NODE(node);
}

bool source_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  return (handle_val == SACN_SOURCE_INVALID) || etcpal_rbtree_find(&sources, &handle_val);
}

// No lock needed as long as called from init
etcpal_error_t start_tick_thread()
{
  shutting_down = false;
  EtcPalThreadParams params = ETCPAL_THREAD_PARAMS_INIT;
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

  process_internal(false);
}

// Takes lock
int process_internal(bool process_manual)
{
  int num_sources_tracked = 0;

  if (sacn_lock())
  {
    // For each source
    EtcPalRbIter source_iter;
    etcpal_rbiter_init(&source_iter);
    SourceState* source = etcpal_rbiter_first(&source_iter, &sources);
    while (source)
    {
      bool source_incremented = false;

      // If this is the kind of source we want to process (manual vs. thread-based)
      if (source->process_manually == process_manual)
      {
        // Increment num_sources_tracked (which only counts either manual or thread-based sources)
        ++num_sources_tracked;

        // If universe list has changed OR universe discovery timer expired
        if (source->universe_list_changed || etcpal_timer_is_expired(&source->universe_discovery_timer))
        {
          // Send universe discovery packet, reset universe discovery timer & universe_list_changed flag
          send_universe_discovery(source);
          source->universe_list_changed = false;
          etcpal_timer_reset(&source->universe_discovery_timer);
        }

        // For each universe of this source
        EtcPalRbIter universe_iter;
        etcpal_rbiter_init(&universe_iter);
        UniverseState* universe = etcpal_rbiter_first(&universe_iter, &source->universes);
        while (universe)
        {
          bool universe_incremented = false;

          // If the Source API is shutting down, cause this universe to terminate (if thread-based)
          if (!process_manual && shutting_down)
            universe->terminating = true;

          // If terminating on this universe
          if (universe->terminating)
          {
            // If termination packet has been sent less than 3 times
            if (universe->num_terminations_sent < 3)
            {
              // Send termination packet
              send_termination(source, universe);

              // If this was the third termination packet sent
              if (universe->num_terminations_sent == 3)
              {
                // Remove universe state
                remove_universe_state(source, &universe, &universe_iter);
                universe_incremented = true;

                // If there are no longer any universes for this source
                if (etcpal_rbtree_size(&source->universes) == 0)
                {
                  // Remove source state
                  remove_source_state(&source, &source_iter);
                  source_incremented = true;
                }
              }
            }
          }
          else
          {
            // If most recent 0x00 data sent < 3 times OR 0x00 keep-alive timer expired
            if ((universe->null_packets_sent_before_suppression < 3) ||
                etcpal_timer_is_expired(&universe->null_keep_alive_timer))
            {
              // Send 0x00 data & reset the keep-alive timer
              send_null_data(source, universe);
              etcpal_timer_reset(&universe->null_keep_alive_timer);
            }
#if SACN_ETC_PRIORITY_EXTENSION
            // If most recent 0xDD data sent < 3 times OR 0xDD keep-alive timer expired
            if ((universe->pap_packets_sent_before_suppression < 3) ||
                etcpal_timer_is_expired(&universe->pap_keep_alive_timer))
            {
              // Send 0xDD data & reset the keep-alive timer
              send_pap_data(source, universe);
              etcpal_timer_reset(&universe->pap_keep_alive_timer);
            }
#endif
          }

          if (source_incremented)
            universe = NULL;  // Stop iterating through old source's universes.
          else if (!universe_incremented)
            universe = etcpal_rbiter_next(&universe_iter);
        }
      }

      if (!source_incremented)
        source = etcpal_rbiter_next(&source_iter);
    }

    sacn_unlock();
  }

  return num_sources_tracked;
}

// Needs lock
void remove_universe_state(const SourceState* source, const UniverseState** universe, EtcPalRbIter* universe_iter)
{
  UniverseState* universe_to_remove = *universe;

  *universe = etcpal_rbiter_next(universe_iter);

  if (universe_to_remove)
  {
    // Update the netints tree first
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
    if (etcpal_rbtree_remove(&source->universes, universe_to_remove) == kEtcPalErrOk)
      FREE_UNIVERSE_STATE(universe_to_remove);
  }
}

// Needs lock
void remove_source_state(const SourceState** source, EtcPalRbIter* source_iter)
{
  SourceState* source_to_remove = *source;
  etcpal_rbtree_clear_with_cb(&(*source)->universes, free_universes_node);
  etcpal_rbtree_clear_with_cb(&(*source)->netints, free_netints_node);
  *source = etcpal_rbiter_next(source_iter);
  if (etcpal_rbtree_remove(&sources, source_to_remove) == kEtcPalErrOk)
    FREE_SOURCE_STATE(source_to_remove);
}

// Needs lock
void send_null_data(const SourceState* source, UniverseState* universe)
{
  // Send multicast and unicast on IPv4 and/or IPv6
  if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
  {
    if (!universe->send_unicast_only)
    {
      send_data_multicast(universe->universe_id, kEtcPalIpTypeV4, universe->null_send_buf, universe->netints,
                          universe->num_netints);
    }

    send_data_unicast(universe->universe_id, kEtcPalIpTypeV4, universe->null_send_buf, universe->unicast_dests,
                      universe->num_unicast_dests);
  }

  if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
  {
    if (!universe->send_unicast_only)
    {
      send_data_multicast(universe->universe_id, kEtcPalIpTypeV6, universe->null_send_buf, universe->netints,
                          universe->num_netints);
    }

    send_data_unicast(universe->universe_id, kEtcPalIpTypeV6, universe->null_send_buf, universe->unicast_dests,
                      universe->num_unicast_dests);
  }

  // Increment sequence number(s)
  ++universe->null_send_buf[SACN_SEQ_OFFSET];
#if SACN_ETC_PRIORITY_EXTENSION
  ++universe->pap_send_buf[SACN_SEQ_OFFSET];
#endif
}

#if SACN_ETC_PRIORITY_EXTENSION
// Needs lock
void send_pap_data(const SourceState* source, UniverseState* universe)
{
  // Send multicast and unicast on IPv4 and/or IPv6
  if ((source->ip_supported == kSacnIpV4Only) || (source->ip_supported == kSacnIpV4AndIpV6))
  {
    if (!universe->send_unicast_only)
    {
      send_data_multicast(universe->universe_id, kEtcPalIpTypeV4, universe->pap_send_buf, universe->netints,
                          universe->num_netints);
    }

    send_data_unicast(universe->universe_id, kEtcPalIpTypeV4, universe->pap_send_buf, universe->unicast_dests,
                      universe->num_unicast_dests);
  }

  if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
  {
    if (!universe->send_unicast_only)
    {
      send_data_multicast(universe->universe_id, kEtcPalIpTypeV6, universe->pap_send_buf, universe->netints,
                          universe->num_netints);
    }

    send_data_unicast(universe->universe_id, kEtcPalIpTypeV6, universe->pap_send_buf, universe->unicast_dests,
                      universe->num_unicast_dests);
  }

  // Increment sequence numbers
  ++universe->null_send_buf[SACN_SEQ_OFFSET];
  ++universe->pap_send_buf[SACN_SEQ_OFFSET];
}
#endif

// Needs lock
void send_termination(const SourceState* source, UniverseState* universe)
{
  // Repurpose null_send_buf for the termination packet
  SET_TERMINATED_OPT(universe->null_send_buf, true);

  // Send the termination packet
  send_null_data(source, universe);

  // Increment the termination counter
  ++universe->num_terminations_sent;
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
        send_data_multicast(SACN_DISCOVERY_UNIVERSE, kEtcPalIpTypeV4, source->universe_discovery_send_buf, netints,
                            num_netints);
      }

      if ((source->ip_supported == kSacnIpV6Only) || (source->ip_supported == kSacnIpV4AndIpV6))
      {
        send_data_multicast(SACN_DISCOVERY_UNIVERSE, kEtcPalIpTypeV6, source->universe_discovery_send_buf, netints,
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
void send_data_multicast(uint16_t universe_id, etcpal_iptype_t ip_type, const uint8_t* send_buf,
                         const EtcPalMcastNetintId* netints, size_t num_netints)
{
  // Determine the multicast destination
  EtcPalSockAddr dest;
  sacn_get_mcast_addr(ip_type, universe_id, &dest.ip);
  dest.port = SACN_PORT;

  // Determine the socket to use
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  if (ip_type == kEtcPalIpTypeV4)
    sock = ipv4_multicast_sock;
  else if (ip_type == kEtcPalIpTypeV6)
    sock = ipv6_multicast_sock;

  // For each network interface
  for (size_t i = 0; netints && (i < num_netints); ++i)
  {
    // If we're able to set this interface as the socket's multicast interface
    if (etcpal_setsockopt(sock, (ip_type == kEtcPalIpTypeV6 ? ETCPAL_IPPROTO_IPV6 : ETCPAL_IPPROTO_IP),
                          ETCPAL_IP_MULTICAST_IF, &netints[i].index, sizeof netints[i].index) == kEtcPalErrOk)
    {
      // Try to send the data (ignore errors)
      etcpal_sendto(sock, send_buf, SACN_MTU, 0, &dest);
    }
  }
}

// Needs lock
void send_data_unicast(uint16_t universe_id, etcpal_iptype_t ip_type, const uint8_t* send_buf,
                       const EtcPalIpAddr* dests, size_t num_dests)
{
  // Determine the socket to use
  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  if (ip_type == kEtcPalIpTypeV4)
    sock = ipv4_unicast_sock;
  else if (ip_type == kEtcPalIpTypeV6)
    sock = ipv6_unicast_sock;

  // For each unicast destination
  for (size_t i = 0; dests && (i < num_dests); ++i)
  {
    // If this destination matches the IP type
    if (dests[i].type == ip_type)
    {
      // Convert destination to SockAddr
      EtcPalSockAddr sockaddr_dest;
      sockaddr_dest.ip = dests[i];
      sockaddr_dest.port = SACN_PORT;

      // Try to send the data (ignore errors)
      etcpal_sendto(sock, send_buf, SACN_MTU, 0, &sockaddr_dest);
    }
  }
}

// Needs lock
int pack_universe_discovery_page(SourceState* source, EtcPalRbIter* universe_iter, int page_number)
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
    // Pack the universe ID
    etcpal_pack_u16b(pcur, universe->universe_id);
    pcur += 2;

    // Increment number of universes packed
    ++num_universes_packed;
  }

  // Update universe count, page, and last page PDU fields
  SET_UNIVERSE_COUNT(source->universe_discovery_send_buf, num_universes_packed);
  SET_PAGE(source->universe_discovery_send_buf, page_number);
  SET_LAST_PAGE(source->universe_discovery_send_buf,
                (uint8_t)(etcpal_rbtree_size(&source->universes) / SACN_UNIVERSE_DISCOVERY_MAX_UNIVERSES_PER_PAGE));

  // Return number of universes packed
  return num_universes_packed;
}

// Needs lock
UniverseState* look_up_universe_state(sacn_source_t handle, uint16_t universe)
{
  // Look up source state and universe state
  SourceState* source_state = etcpal_rbtree_find(&sources, &handle);
  if (source_state)
    return (UniverseState*)etcpal_rbtree_find(&source_state->universes, &universe);

  // Source not found, return NULL
  return NULL;
}

// Needs lock
void update_data(uint8_t* send_buf, const uint8_t* new_data, size_t new_data_size, bool force_sync)
{
  // Set force sync flag
  SET_FORCE_SYNC_OPT(send_buf, force_sync);

  // Set the property value count field (1 + number of slots in packet)
  SET_PROPERTY_VALUE_COUNT(send_buf, 1 + new_data_size);

  // Copy data into the send buffer immediately after the start code
  memcpy(&send_buf[SACN_DATA_HEADER_SIZE], new_data, new_data_size);
}
