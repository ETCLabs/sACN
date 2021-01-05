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

#include "sacn/receiver.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "etcpal/acn_pdu.h"
#include "etcpal/acn_rlp.h"
#include "etcpal/common.h"
#include "etcpal/rbtree.h"
#include "sacn/private/common.h"
#include "sacn/private/source_loss.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"
#include "sacn/private/receiver.h"
#include "sacn/private/sockets.h"
#include "sacn/private/util.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

// TODO: Sources Found was ripped out - now tie up loose ends and implement new start/end notifs

/***************************** Private constants *****************************/

static const EtcPalThreadParams kReceiverThreadParams = {SACN_RECEIVER_THREAD_PRIORITY, SACN_RECEIVER_THREAD_STACK,
                                                         "sACN Receive Thread", NULL};

#define SACN_PERIODIC_INTERVAL 120

/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if SACN_DYNAMIC_MEM
#define ALLOC_RECEIVER() malloc(sizeof(SacnReceiver))
#define ALLOC_TRACKED_SOURCE() malloc(sizeof(SacnTrackedSource))
#define FREE_RECEIVER(ptr) \
  do                       \
  {                        \
    if (ptr->netints)      \
    {                      \
      free(ptr->netints);  \
    }                      \
    free(ptr);             \
  } while (0)
#define FREE_TRACKED_SOURCE(ptr) free(ptr)
#else
#define ALLOC_RECEIVER() etcpal_mempool_alloc(sacnrecv_receivers)
#define ALLOC_TRACKED_SOURCE() etcpal_mempool_alloc(sacnrecv_tracked_sources)
#define FREE_RECEIVER(ptr) etcpal_mempool_free(sacnrecv_receivers, ptr)
#define FREE_TRACKED_SOURCE(ptr) etcpal_mempool_free(sacnrecv_tracked_sources, ptr)
#endif

/****************************** Private types ********************************/

typedef struct PeriodicCallbacks
{
  const SourcesLostNotification* sources_lost_arr;
  size_t num_sources_lost;
  const SamplingStartedNotification* sampling_started_arr;
  size_t num_sampling_started;
  const SamplingEndedNotification* sampling_ended_arr;
  size_t num_sampling_ended;
} PeriodicCallbacks;

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(sacnrecv_receivers, SacnReceiver, SACN_RECEIVER_MAX_UNIVERSES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_tracked_sources, SacnTrackedSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_rb_nodes, EtcPalRbNode, SACN_RECEIVER_MAX_RB_NODES);
#endif

static struct SacnRecvState
{
  sacn_standard_version_t version_listening;
  uint32_t expired_wait;

  IntHandleManager handle_mgr;
  EtcPalRbTree receivers;
  EtcPalRbTree receivers_by_universe;
} receiver_state;

/*********************** Private function prototypes *************************/

// Receiver creation and destruction
static etcpal_error_t validate_receiver_config(const SacnReceiverConfig* config);
static etcpal_error_t initialize_receiver_netints(SacnReceiver* receiver, SacnMcastInterface* netints,
                                                  size_t num_netints);
static etcpal_error_t create_new_receiver(const SacnReceiverConfig* config, SacnMcastInterface* netints,
                                          size_t num_netints, SacnReceiver** new_receiver);
static etcpal_error_t assign_receiver_to_thread(SacnReceiver* receiver, const SacnReceiverConfig* config);
static etcpal_error_t insert_receiver_into_maps(SacnReceiver* receiver);
static etcpal_error_t start_receiver_thread(SacnRecvThreadContext* recv_thread_context);

static void remove_receiver_from_thread(SacnReceiver* receiver, bool close_socket_now);
static void remove_receiver_from_maps(SacnReceiver* receiver);

static void sacn_receive_thread(void* arg);

// Receiving incoming data
static void handle_incoming(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                            const EtcPalSockAddr* from_addr);
static void handle_sacn_data_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                                    const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr, bool draft);
static void process_null_start_code(const SacnReceiver* receiver, SacnTrackedSource* src,
                                    SourcePapLostNotification* source_pap_lost, bool* notify);
static void process_pap(const SacnReceiver* receiver, SacnTrackedSource* src, bool* notify);
static void process_new_source_data(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const SacnHeaderData* header,
                                    uint8_t seq, SacnTrackedSource** new_source,
                                    SourceLimitExceededNotification* source_limit_exceeded, bool* notify);
static bool check_sequence(int8_t new_seq, int8_t old_seq);
static void deliver_receive_callbacks(const EtcPalSockAddr* from_addr, const EtcPalUuid* sender_cid,
                                      const SacnHeaderData* header,
                                      SourceLimitExceededNotification* source_limit_exceeded,
                                      SourcePapLostNotification* source_pap_lost,
                                      UniverseDataNotification* universe_data);

// Process periodic timeout functionality
static void process_receivers(SacnRecvThreadContext* recv_thread_context);
static void process_receiver_sources(sacn_thread_id_t thread_id, SacnReceiver* receiver,
                                     SourcesLostNotification* sources_lost);
static bool check_source_timeouts(SacnTrackedSource* src, SacnSourceStatusLists* status_lists);
static void update_source_status(SacnTrackedSource* src, SacnSourceStatusLists* status_lists);
static void deliver_periodic_callbacks(const PeriodicCallbacks* periodic_callbacks);

// Tree node management
static int tracked_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare_by_universe(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* node_alloc(void);
static void node_dealloc(EtcPalRbNode* node);
static void source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static bool receiver_handle_in_use(int handle_val, void* cookie);

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Receiver module. Internal function called from sacn_init(). */
etcpal_error_t sacn_receiver_init(void)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !SACN_DYNAMIC_MEM
  res |= etcpal_mempool_init(sacnrecv_receivers);
  res |= etcpal_mempool_init(sacnrecv_tracked_sources);
  res |= etcpal_mempool_init(sacnrecv_rb_nodes);
#endif

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&receiver_state.receivers, receiver_compare, node_alloc, node_dealloc);
    etcpal_rbtree_init(&receiver_state.receivers_by_universe, receiver_compare_by_universe, node_alloc, node_dealloc);
    init_int_handle_manager(&receiver_state.handle_mgr, receiver_handle_in_use, NULL);
    receiver_state.version_listening = kSacnStandardVersionAll;
    receiver_state.expired_wait = SACN_DEFAULT_EXPIRED_WAIT_MS;
  }
  else
  {
    memset(&receiver_state, 0, sizeof receiver_state);
  }

  return res;
}

/* Deinitialize the sACN Receiver module. Internal function called from sacn_deinit(). */
void sacn_receiver_deinit(void)
{
  // Stop all receive threads
  for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
  {
    SacnRecvThreadContext* thread_context = get_recv_thread_context(i);
    if (thread_context && thread_context->running)
    {
      thread_context->running = false;
      etcpal_thread_join(&thread_context->thread_handle);
      sacn_cleanup_dead_sockets(thread_context);
    }
  }

  // Clear out the rest of the state tracking
  etcpal_rbtree_clear_with_cb(&receiver_state.receivers, universe_tree_dealloc);
  etcpal_rbtree_clear(&receiver_state.receivers_by_universe);
  memset(&receiver_state, 0, sizeof receiver_state);
}

/**
 * @brief Initialize an sACN Receiver Config struct to default values.
 *
 * @param[out] config Config struct to initialize.
 */
void sacn_receiver_config_init(SacnReceiverConfig* config)
{
  if (config)
  {
    memset(config, 0, sizeof(SacnReceiverConfig));
    config->source_count_max = SACN_RECEIVER_INFINITE_SOURCES;
  }
}

/**
 * @brief Create a new sACN receiver to listen for sACN data on a universe.
 *
 * A sACN receiver can listen on one universe at a time, and each universe can only be listened to
 * by one receiver at at time.
 *
 * After this call completes successfully, the receiver is in a sampling period for the universe and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for the universe.
 *
 * Note that a receiver is considered as successfully created if it is able to successfully use any of the
 * network interfaces passed in.  This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] config Configuration parameters for the sACN receiver to be created.
 * @param[out] handle Filled in on success with a handle to the sACN receiver.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Receiver created successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * @return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * @return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_create(const SacnReceiverConfig* config, sacn_receiver_t* handle,
                                    SacnMcastInterface* netints, size_t num_netints)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;

  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = validate_receiver_config(config);
  if (res != kEtcPalErrOk)
    return res;

  if (sacn_lock())
  {
    // First check to see if we are already listening on this universe.
    SacnReceiverKeys lookup_keys;
    lookup_keys.universe = config->universe_id;
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers_by_universe, &lookup_keys);
    if (receiver)
    {
      res = kEtcPalErrExists;
      receiver = NULL;
    }

    if (res == kEtcPalErrOk)
      res = create_new_receiver(config, netints, num_netints, &receiver);

    if (res == kEtcPalErrOk)
      res = assign_receiver_to_thread(receiver, config);

    // Insert the new universe into the map.
    if (res == kEtcPalErrOk)
      res = insert_receiver_into_maps(receiver);

    if (res == kEtcPalErrOk)
    {
      *handle = receiver->keys.handle;
    }
    else
    {
      if (receiver)
      {
        remove_receiver_from_maps(receiver);
        remove_receiver_from_thread(receiver, true);
        FREE_RECEIVER(receiver);
      }
    }
    sacn_unlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Destroy a sACN receiver instance.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 *
 * @param[in] handle Handle to the receiver to destroy.
 * @return #kEtcPalErrOk: Receiver destroyed successfully.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_destroy(sacn_receiver_t handle)
{
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrOk;
  if (sacn_lock())
  {
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers, &handle);
    if (receiver)
    {
      remove_receiver_from_thread(receiver, false);
      etcpal_rbtree_clear_with_cb(&receiver->sources, source_tree_dealloc);
      remove_receiver_from_maps(receiver);
      FREE_RECEIVER(receiver);
    }
    else
    {
      res = kEtcPalErrNotFound;
    }
    sacn_unlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }
  return res;
}

/**
 * @brief Get the universe on which a sACN receiver is currently listening.
 *
 * @param[in] handle Handle to the receiver that we want to query.
 * @param[out] universe_id The retrieved universe.
 * @return #kEtcPalErrOk: Universe retrieved successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_get_universe(sacn_receiver_t handle, uint16_t* universe_id)
{
  if (!sacn_initialized())
    return kEtcPalErrNotInit;
  if (universe_id == NULL)
    return kEtcPalErrInvalid;

  etcpal_error_t res = kEtcPalErrOk;
  if (sacn_lock())
  {
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers, &handle);
    if (receiver)
      *universe_id = receiver->keys.universe;
    else
      res = kEtcPalErrNotFound;

    sacn_unlock();
  }

  return res;
}

/**
 * @brief Change the universe on which an sACN receiver is listening.
 *
 * An sACN receiver can only listen on one universe at a time. After this call completes successfully, the receiver is
 * in a sampling period for the universe and will provide SamplingPeriodStarted() and SamplingPeriodEnded()
 * notifications, as well as UniverseData() notifications as packets are received for the universe. If this call fails,
 * the caller must call sacn_receiver_destroy for the receiver, because the receiver may be in an invalid state.
 *
 * @param[in] handle Handle to the receiver for which to change the universe.
 * @param[in] new_universe_id New universe number that this receiver should listen to.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrExists: A receiver already exists which is listening on the specified new universe.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_change_universe(sacn_receiver_t handle, uint16_t new_universe_id)
{
  if (!UNIVERSE_ID_VALID(new_universe_id))
    return kEtcPalErrInvalid;

  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrOk;
  if (sacn_lock())
  {
    // First check to see if there is already a receiver listening on this universe.
    SacnReceiverKeys lookup_keys;
    lookup_keys.universe = new_universe_id;
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers_by_universe, &lookup_keys);
    if (receiver)
    {
      res = kEtcPalErrExists;
    }

    // Find the receiver to change the universe for.
    if (res == kEtcPalErrOk)
    {
      receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers, &handle);
      if (!receiver)
        res = kEtcPalErrNotFound;
    }

    // Clear termination sets and sources since they only pertain to the old universe.
    if (res == kEtcPalErrOk)
    {
      clear_term_set_list(receiver->term_sets);
      receiver->term_sets = NULL;
      res = etcpal_rbtree_clear_with_cb(&receiver->sources, source_tree_dealloc);
    }

    // Update the receiver's socket and subscription.
    if (res == kEtcPalErrOk)
    {
      if (receiver->ipv4_socket != ETCPAL_SOCKET_INVALID)
        sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv4_socket, false);
      if (receiver->ipv6_socket != ETCPAL_SOCKET_INVALID)
        sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv6_socket, false);

      if (supports_ipv4(receiver->ip_supported))
      {
        res = sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV4, new_universe_id, receiver->netints,
                                       receiver->num_netints, &receiver->ipv4_socket);

        if ((res == kEtcPalErrNoNetints) && supports_ipv6(receiver->ip_supported))
          res = kEtcPalErrOk;  // Try IPv6.
      }
    }

    if (res == kEtcPalErrOk)
    {
      if (supports_ipv6(receiver->ip_supported))
      {
        res = sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV6, new_universe_id, receiver->netints,
                                       receiver->num_netints, &receiver->ipv6_socket);
      }
    }

    // Update receiver key and position in receiver_state.receivers_by_universe.
    if (res == kEtcPalErrOk)
    {
      etcpal_rbtree_remove(&receiver_state.receivers_by_universe, receiver);
      receiver->keys.universe = new_universe_id;
      res = etcpal_rbtree_insert(&receiver_state.receivers_by_universe, receiver);
    }

    // Begin the sampling period.
    if (res == kEtcPalErrOk)
    {
      receiver->sampling = true;
      receiver->notified_sampling_started = false;
      etcpal_timer_start(&receiver->sample_timer, SAMPLE_TIME);
    }

    sacn_unlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/**
 * @brief Resets the underlying network sockets and packet receipt state for the sACN receiver..
 *
 * This is typically used when the application detects that the list of networking interfaces has changed.
 *
 * After this call completes successfully, the receiver is in a sampling period for the universe and will provide
 * SamplingPeriodStarted() and SamplingPeriodEnded() notifications, as well as UniverseData() notifications as packets
 * are received for the universe. If this call fails, the caller must call sacn_receiver_destroy for the receiver,
 * because the receiver may be in an invalid state.
 *
 * Note that the networking reset is considered successful if it is able to successfully use any of the
 * network interfaces passed in. This will only return #kEtcPalErrNoNetints if none of the interfaces work.
 *
 * @param[in] handle Handle to the receiver for which to reset the networking.
 * @param[in, out] netints Optional. If non-NULL, this is the list of interfaces the application wants to use, and the
 * status codes are filled in.  If NULL, all available interfaces are tried.
 * @param[in, out] num_netints Optional. The size of netints, or 0 if netints is NULL.
 * @return #kEtcPalErrOk: Universe changed successfully.
 * @return #kEtcPalErrNoNetints: None of the network interfaces provided were usable by the library.
 * @return #kEtcPalErrInvalid: Invalid parameter provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_reset_networking(sacn_receiver_t handle, SacnMcastInterface* netints, size_t num_netints)
{
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(netints);
  ETCPAL_UNUSED_ARG(num_netints);

  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  return kEtcPalErrNotImpl;
}

/**
 * @brief Obtain the statuses of a receiver's network interfaces.
 *
 * @param[in] handle Handle to the receiver for which to obtain the list of network interfaces.
 * @param[out] netints A pointer to an application-owned array where the network interface list will be written.
 * @param[in] netints_size The size of the provided netints array.
 * @return The total number of network interfaces for the receiver. If this is greater than netints_size, then only
 * netints_size addresses were written to the netints array. If the receiver was not found, 0 is returned.
 */
size_t sacn_receiver_get_network_interfaces(sacn_receiver_t handle, SacnMcastInterface* netints, size_t netints_size)
{
  return 0;  // TODO
}

/**
 * @brief Set the current version of the sACN standard to which the module is listening.
 *
 * This is a global option across all listening receivers.
 *
 * @param[in] version Version of sACN to listen to.
 */
void sacn_receiver_set_standard_version(sacn_standard_version_t version)
{
  if (!sacn_initialized())
    return;

  if (sacn_lock())
  {
    receiver_state.version_listening = version;
    sacn_unlock();
  }
}

/**
 * @brief Get the current version of the sACN standard to which the module is listening.
 *
 * This is a global option across all listening receivers.
 *
 * @return Version of sACN to which the module is listening, or #kSacnStandardVersionNone if the module is
 *         not initialized.
 */
sacn_standard_version_t sacn_receiver_get_standard_version()
{
  sacn_standard_version_t res = kSacnStandardVersionNone;

  if (!sacn_initialized())
    return res;

  if (sacn_lock())
  {
    res = receiver_state.version_listening;
    sacn_unlock();
  }
  return res;
}

/**
 * @brief Set the expired notification wait time.
 *
 * The library will wait at least this long after a source loss condition has been encountered before
 * sending a @ref SacnSourcesLostCallback "sources_lost()" notification. However, the wait may be
 * longer due to the source loss algorithm (see @ref source_loss_behavior).
 *
 * @param[in] wait_ms Wait time in milliseconds.
 */
void sacn_receiver_set_expired_wait(uint32_t wait_ms)
{
  if (!sacn_initialized())
    return;

  if (sacn_lock())
  {
    receiver_state.expired_wait = wait_ms;
    sacn_unlock();
  }
}

/**
 * @brief Get the current value of the expired notification wait time.
 *
 * The library will wait at least this long after a source loss condition has been encountered before
 * sending a @ref SacnSourcesLostCallback "sources_lost()" notification. However, the wait may be
 * longer due to the source loss algorithm (see @ref source_loss_behavior).
 *
 * @return Wait time in milliseconds.
 */
uint32_t sacn_receiver_get_expired_wait()
{
  uint32_t res = SACN_DEFAULT_EXPIRED_WAIT_MS;

  if (!sacn_initialized())
    return res;

  if (sacn_lock())
  {
    res = receiver_state.expired_wait;
    sacn_unlock();
  }
  return res;
}

/**************************************************************************************************
 * Internal helpers for receiver creation and destruction
 *************************************************************************************************/

/*
 * Make sure the values provided in an SacnReceiverConfig struct are valid.
 */
etcpal_error_t validate_receiver_config(const SacnReceiverConfig* config)
{
  SACN_ASSERT(config);

  if (!UNIVERSE_ID_VALID(config->universe_id) || !config->callbacks.universe_data || !config->callbacks.sources_lost ||
      !config->callbacks.sampling_period_ended)
  {
    return kEtcPalErrInvalid;
  }

  return kEtcPalErrOk;
}

/*
 * Initialize a SacnReceiver's network interface data.
 */
etcpal_error_t initialize_receiver_netints(SacnReceiver* receiver, SacnMcastInterface* netints, size_t num_netints)
{
  size_t num_valid_netints = 0u;
  etcpal_error_t result = sacn_validate_netint_config(netints, num_netints, &num_valid_netints);
  if (result != kEtcPalErrOk)
    return result;

  if (netints)
  {
#if SACN_DYNAMIC_MEM
    EtcPalMcastNetintId* calloc_result = calloc(num_valid_netints, sizeof(EtcPalMcastNetintId));

    if (calloc_result)
    {
      receiver->netints = calloc_result;
    }
    else
    {
      result = kEtcPalErrNoMem;
    }
#else
    if (num_netints > SACN_MAX_NETINTS)
    {
      result = kEtcPalErrNoMem;
    }
#endif

    if (result == kEtcPalErrOk)
    {
      for (size_t read_index = 0u, write_index = 0u; read_index < num_netints; ++read_index)
      {
        if (netints[read_index].status == kEtcPalErrOk)
        {
          memcpy(&receiver->netints[write_index], &netints[read_index].iface, sizeof(EtcPalMcastNetintId));
          ++write_index;
        }
      }
    }
  }
#if SACN_DYNAMIC_MEM
  else
  {
    receiver->netints = NULL;
  }
#endif

  if (result == kEtcPalErrOk)
  {
    receiver->num_netints = num_valid_netints;
  }

  return result;
}

/*
 * Allocate a new receiver instances and do essential first initialization, in preparation for
 * creating the sockets and subscriptions.
 *
 * [in] config Receiver configuration data.
 * Returns the new initialized receiver instance, or NULL if out of memory.
 */
etcpal_error_t create_new_receiver(const SacnReceiverConfig* config, SacnMcastInterface* netints, size_t num_netints,
                                   SacnReceiver** new_receiver)
{
  SACN_ASSERT(config);

  sacn_receiver_t new_handle = get_next_int_handle(&receiver_state.handle_mgr, -1);
  if (new_handle == SACN_RECEIVER_INVALID)
    return kEtcPalErrNoMem;

  SacnReceiver* receiver = ALLOC_RECEIVER();
  if (!receiver)
    return kEtcPalErrNoMem;

  receiver->keys.handle = new_handle;
  receiver->keys.universe = config->universe_id;
  receiver->thread_id = SACN_THREAD_ID_INVALID;

  receiver->ipv4_socket = ETCPAL_SOCKET_INVALID;
  receiver->ipv6_socket = ETCPAL_SOCKET_INVALID;

  etcpal_error_t initialize_receiver_netints_result = initialize_receiver_netints(receiver, netints, num_netints);
  if (initialize_receiver_netints_result != kEtcPalErrOk)
  {
    FREE_RECEIVER(receiver);
    return initialize_receiver_netints_result;
  }

  receiver->sampling = true;
  receiver->notified_sampling_started = false;
  etcpal_timer_start(&receiver->sample_timer, SAMPLE_TIME);
  receiver->suppress_limit_exceeded_notification = false;
  etcpal_rbtree_init(&receiver->sources, tracked_source_compare, node_alloc, node_dealloc);
  receiver->term_sets = NULL;

  receiver->filter_preview_data = ((config->flags & SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA) != 0);

  receiver->callbacks = config->callbacks;

  receiver->source_count_max = config->source_count_max;

  receiver->ip_supported = config->ip_supported;

  receiver->next = NULL;

  *new_receiver = receiver;

  return kEtcPalErrOk;
}

/*
 * Pick a thread for the receiver based on current load balancing, create the receiver's sockets,
 * and assign it to that thread.
 *
 * [in,out] receiver Receiver instance to assign.
 * [in] config Config that was used to create the receiver instance.
 * Returns error code indicating the result of the operations.
 */
etcpal_error_t assign_receiver_to_thread(SacnReceiver* receiver, const SacnReceiverConfig* config)
{
  SacnRecvThreadContext* assigned_thread = NULL;

  // Assign this receiver to the thread with the lowest number of receivers currently
  for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
  {
    SacnRecvThreadContext* context = get_recv_thread_context(i);
    if (context)
    {
      if (!assigned_thread || (context->num_receivers < assigned_thread->num_receivers))
      {
        receiver->thread_id = i;
        assigned_thread = context;
      }
    }
  }

  SACN_ASSERT(assigned_thread);

  etcpal_error_t res = kEtcPalErrOk;

  if (supports_ipv4(receiver->ip_supported))
  {
    res = sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV4, config->universe_id, receiver->netints,
                                   receiver->num_netints, &receiver->ipv4_socket);

    if ((res == kEtcPalErrNoNetints) && supports_ipv6(receiver->ip_supported))
      res = kEtcPalErrOk;  // Try IPv6.
  }

  if ((res == kEtcPalErrOk) && supports_ipv6(receiver->ip_supported))
  {
    res = sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV6, config->universe_id, receiver->netints,
                                   receiver->num_netints, &receiver->ipv6_socket);
  }

  if ((res == kEtcPalErrOk) && !assigned_thread->running)
  {
    res = start_receiver_thread(assigned_thread);
    if (res != kEtcPalErrOk)
    {
      if (receiver->ipv4_socket != ETCPAL_SOCKET_INVALID)
        sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv4_socket, true);
      if (receiver->ipv6_socket != ETCPAL_SOCKET_INVALID)
        sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv6_socket, true);
    }
  }

  if (res == kEtcPalErrOk)
  {
    // Append the receiver to the thread list
    add_receiver_to_list(assigned_thread, receiver);
  }

  return res;
}

/*
 * Add a receiver to the maps that are used to track receivers globally.
 *
 * [in] receiver Receiver instance to add.
 * Returns error code indicating the result of the operations.
 */
etcpal_error_t insert_receiver_into_maps(SacnReceiver* receiver)
{
  etcpal_error_t res = etcpal_rbtree_insert(&receiver_state.receivers, receiver);
  if (res == kEtcPalErrOk)
  {
    res = etcpal_rbtree_insert(&receiver_state.receivers_by_universe, receiver);
    if (res != kEtcPalErrOk)
      etcpal_rbtree_remove(&receiver_state.receivers, receiver);
  }
  return res;
}

/*
 * Start a new thread to process receiver state. The thread is associated with a specific
 * SacnRecvThreadContext instance.
 *
 * [in,out] recv_thread_context Thread context corresponding to the thread to start.
 * Returns error code indicating the result of the thread start operation.
 */
etcpal_error_t start_receiver_thread(SacnRecvThreadContext* recv_thread_context)
{
  recv_thread_context->running = true;
  etcpal_error_t create_res = etcpal_thread_create(&recv_thread_context->thread_handle, &kReceiverThreadParams,
                                                   sacn_receive_thread, recv_thread_context);
  if (create_res != kEtcPalErrOk)
  {
    recv_thread_context->running = false;
  }
  return create_res;
}

/*
 * Remove a receiver instance from a receiver thread. After this completes, the thread will no
 * longer process timeouts for that receiver.
 *
 * [in,out] receiver Receiver to remove.
 * [in] close_socket_now Whether to close the socket immediately (e.g. on full library shutdown).
 * Returns error code indicating the result of the removal operation.
 */
void remove_receiver_from_thread(SacnReceiver* receiver, bool close_socket_now)
{
  SacnRecvThreadContext* context = get_recv_thread_context(receiver->thread_id);
  if (context)
  {
    if (receiver->ipv4_socket != ETCPAL_SOCKET_INVALID)
      sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv4_socket, close_socket_now);
    if (receiver->ipv6_socket != ETCPAL_SOCKET_INVALID)
      sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv6_socket, close_socket_now);

    remove_receiver_from_list(context, receiver);
  }
}

/*
 * Remove a receiver instance from the maps that are used to track receivers globally.
 *
 * [in] receiver Receiver to remove.
 */
void remove_receiver_from_maps(SacnReceiver* receiver)
{
  etcpal_rbtree_remove(&receiver_state.receivers_by_universe, receiver);
  etcpal_rbtree_remove(&receiver_state.receivers, receiver);
}

/*
 * The receiver thread function. Receives and forwards sACN data from the network and processes
 * periodic timeouts for sACN receivers.
 */
void sacn_receive_thread(void* arg)
{
  SacnRecvThreadContext* context = (SacnRecvThreadContext*)arg;
  SACN_ASSERT(context);

  // Create the poll context
  etcpal_error_t init_res = etcpal_poll_context_init(&context->poll_context);
  if (init_res != kEtcPalErrOk)
  {
    SACN_LOG_CRIT(
        "Could not create a socket poll context for sACN: '%s'. sACN Receive functionality will not work properly.",
        etcpal_strerror(init_res));
    return;
  }

  EtcPalTimer periodic_timer;
  etcpal_timer_start(&periodic_timer, SACN_PERIODIC_INTERVAL);

  while (context->running)
  {
    if (sacn_lock())
    {
      sacn_add_pending_sockets(context);
      sacn_cleanup_dead_sockets(context);
      sacn_unlock();
    }

    SacnReadResult read_result;
    etcpal_error_t read_res = sacn_read(context, &read_result);
    if (read_res == kEtcPalErrOk)
    {
      handle_incoming(context->thread_id, read_result.data, read_result.data_len, &read_result.from_addr);
    }
    else if (read_res != kEtcPalErrTimedOut)
    {
      if (read_res != kEtcPalErrNoSockets)
      {
        SACN_LOG_WARNING("Error occurred while attempting to read sACN incoming data: '%s'.",
                         etcpal_strerror(read_res));
      }
      etcpal_thread_sleep(SACN_RECEIVER_READ_TIMEOUT_MS);
    }

    if (etcpal_timer_is_expired(&periodic_timer))
    {
      process_receivers(context);
      etcpal_timer_reset(&periodic_timer);
    }
  }

  // Destroy the poll context
  etcpal_poll_context_deinit(&context->poll_context);
}

/**************************************************************************************************
 * Internal helpers for processing incoming sACN data
 *************************************************************************************************/

/*
 * Handle an incoming data packet on a receiver socket.
 *
 * [in] thread_id ID for the thread in which the incoming data was received.
 * [in] data Incoming data buffer.
 * [in] datalen Size of data buffer.
 * [in] from_addr Network address from which the data was received.
 */
void handle_incoming(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen, const EtcPalSockAddr* from_addr)
{
  AcnUdpPreamble preamble;
  if (!acn_parse_udp_preamble(data, datalen, &preamble))
    return;

  AcnRootLayerPdu rlp;
  AcnPdu lpdu = ACN_PDU_INIT;
  while (acn_parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &lpdu))
  {
    if (rlp.vector == ACN_VECTOR_ROOT_E131_DATA && (receiver_state.version_listening == kSacnStandardVersionPublished ||
                                                    receiver_state.version_listening == kSacnStandardVersionAll))
    {
      handle_sacn_data_packet(thread_id, rlp.pdata, rlp.data_len, &rlp.sender_cid, from_addr, false);
    }
    else if (rlp.vector == ACN_VECTOR_ROOT_DRAFT_E131_DATA &&
             (receiver_state.version_listening == kSacnStandardVersionDraft ||
              receiver_state.version_listening == kSacnStandardVersionAll))
    {
      handle_sacn_data_packet(thread_id, rlp.pdata, rlp.data_len, &rlp.sender_cid, from_addr, true);
    }
  }
}

/*
 * Handle an sACN Data packet that has been unpacked from a Root Layer PDU.
 *
 * [in] thread_id ID for the thread in which the data packet was received.
 * [in] data Buffer containing the data packet.
 * [in] datalen Size of buffer.
 * [in] sender_cid CID from which the data was received.
 * [in] from_addr Network address from which the data was received.
 * [in] draft Whether the data packet is in draft or ratified sACN format.
 */
void handle_sacn_data_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                             const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr, bool draft)
{
  UniverseDataNotification* universe_data = get_universe_data(thread_id);
  SourceLimitExceededNotification* source_limit_exceeded = get_source_limit_exceeded(thread_id);
  SourcePapLostNotification* source_pap_lost = get_source_pap_lost(thread_id);
  if (!universe_data || !source_limit_exceeded || !source_pap_lost)
  {
    SACN_LOG_ERR("Could not allocate memory for incoming sACN data packet!");
    return;
  }

  uint8_t seq;
  bool is_termination_packet;
  bool parse_res = false;

  SacnHeaderData* header = &universe_data->header;
  header->cid = *sender_cid;

  if (draft)
  {
    parse_res =
        parse_draft_sacn_data_packet(data, datalen, header, &seq, &is_termination_packet, &universe_data->pdata);
  }
  else
  {
    parse_res = parse_sacn_data_packet(data, datalen, header, &seq, &is_termination_packet, &universe_data->pdata);
  }

  if (!parse_res)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(sender_cid, cid_str);
      SACN_LOG_WARNING("Ignoring malformed %ssACN data packet from component %s", draft ? "Draft " : "", cid_str);
    }
    return;
  }

  if (sacn_lock())
  {
    SacnReceiverKeys lookup_keys;
    lookup_keys.universe = header->universe_id;
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers_by_universe, &lookup_keys);
    if (!receiver)
    {
      // We are not listening to this universe.
      sacn_unlock();
      return;
    }

    bool notify = false;
    SacnTrackedSource* src = (SacnTrackedSource*)etcpal_rbtree_find(&receiver->sources, sender_cid);
    if (src)
    {
      // Check to see if the 'stream terminated' bit is set in the options
      if (is_termination_packet)
      {
        src->terminated = true;
        etcpal_timer_start(&src->packet_timer, 0);
      }
      // This also handles the case where the source was already terminated in a previous packet
      // but not yet removed.
      if (src->terminated)
      {
        sacn_unlock();
        return;
      }

      if (!check_sequence(seq, src->seq))
      {
        // Drop the packet
        sacn_unlock();
        return;
      }
      src->seq = seq;

      // Based on the start code, update the timers.
      if (header->start_code == SACN_STARTCODE_DMX)
      {
        process_null_start_code(receiver, src, source_pap_lost, &notify);
      }
#if SACN_ETC_PRIORITY_EXTENSION
      else if (header->start_code == SACN_STARTCODE_PRIORITY)
      {
        process_pap(receiver, src, &notify);
      }
#endif
      else if (header->start_code != SACN_STARTCODE_PRIORITY)
      {
        notify = true;
      }
    }
    else if (!is_termination_packet)
    {
      process_new_source_data(receiver, sender_cid, header, seq, &src, source_limit_exceeded, &notify);
    }
    // Else we weren't tracking this source before and it is a termination packet. Ignore.

    if (src)
    {
      if (header->preview && receiver->filter_preview_data)
      {
        notify = false;
      }

      if (notify)
      {
        universe_data->callback = receiver->callbacks.universe_data;
        universe_data->handle = receiver->keys.handle;
        universe_data->universe = receiver->keys.universe;
        universe_data->context = receiver->callbacks.context;
      }
    }

    sacn_unlock();
  }

  // Deliver callbacks if applicable.
  deliver_receive_callbacks(from_addr, sender_cid, header, source_limit_exceeded, source_pap_lost, universe_data);
}

/*
 * Process the timers and logic upon receiving NULL START Code data from an existing source.
 *
 * [in] receiver Receiver on which this source data was received.
 * [in,out] src Existing source from which this data was received - state tracking is updated.
 * [out] source_pap_lost Notification data to deliver if a PAP lost condition should be forwarded
 *                       to the app.
 * [out] notify Whether or not to forward the data to the user in a notification.
 */
void process_null_start_code(const SacnReceiver* receiver, SacnTrackedSource* src,
                             SourcePapLostNotification* source_pap_lost, bool* notify)
{
  *notify = true;  // Notify universe data during and after the sampling period.

  // No matter how valid, we got something.
  src->dmx_received_since_last_tick = true;
  etcpal_timer_start(&src->packet_timer, SOURCE_LOSS_TIMEOUT);

#if SACN_ETC_PRIORITY_EXTENSION
  switch (src->recv_state)
  {
    case kRecvStateWaitingForDmx:
      // We had previously received PAP, were waiting for DMX and got it.
      if (receiver->sampling)
      {
        // We are in the sample period - notify immediately.
        src->recv_state = kRecvStateHaveDmxAndPap;
      }
      else
      {
        // Now we wait for one more PAP packet before notifying.
        src->recv_state = kRecvStateWaitingForPap;
        *notify = false;
      }
      break;
    case kRecvStateWaitingForPap:
      if (etcpal_timer_is_expired(&src->pap_timer))
      {
        // Our per-address-priority waiting period has expired. Keep the timer going in case the
        // source starts sending PAP later.
        src->recv_state = kRecvStateHaveDmxOnly;
        etcpal_timer_start(&src->pap_timer, SOURCE_LOSS_TIMEOUT);
      }
      else
      {
        // We've received a DMX packet during our per-address-priority waiting period. Don't notify.
        *notify = false;
      }
      break;
    case kRecvStateHaveDmxOnly:
      // More DMX, nothing to see here
      break;
    case kRecvStateHaveDmxAndPap:
      if (etcpal_timer_is_expired(&src->pap_timer))
      {
        // Source stopped sending PAP but is still sending DMX.
        if (*notify)
        {
          // In this case, also notify the source_pap_lost callback.
          source_pap_lost->callback = receiver->callbacks.source_pap_lost;
          source_pap_lost->source.cid = src->cid;
          ETCPAL_MSVC_NO_DEP_WRN strcpy(source_pap_lost->source.name, src->name);
          source_pap_lost->context = receiver->callbacks.context;
          source_pap_lost->handle = receiver->keys.handle;
          source_pap_lost->universe = receiver->keys.universe;
        }

        src->recv_state = kRecvStateHaveDmxOnly;
      }
      break;
    default:
      break;
  }
#endif
}

#if SACN_ETC_PRIORITY_EXTENSION
/*
 * Process the timers and logic upon receiving per-address priority data from an existing source.
 *
 * [in] receiver Receiver instance for which PAP data was received.
 * [in,out] src Existing source from which this PAP data was received - state tracking is updated.
 * [out] notify Whether or not to forward the data to the user in a notification.
 */
void process_pap(const SacnReceiver* receiver, SacnTrackedSource* src, bool* notify)
{
  *notify = true;

  switch (src->recv_state)
  {
    case kRecvStateWaitingForDmx:
      // Still waiting for DMX - ignore PAP packets until we've seen at least one DMX packet.
      *notify = false;
      etcpal_timer_reset(&src->pap_timer);
      break;
    case kRecvStateWaitingForPap:
    case kRecvStateHaveDmxOnly:
      src->recv_state = kRecvStateHaveDmxAndPap;
      etcpal_timer_start(&src->pap_timer, SOURCE_LOSS_TIMEOUT);
      break;
    case kRecvStateHaveDmxAndPap:
      etcpal_timer_reset(&src->pap_timer);
      break;
    default:
      break;
  }
}
#endif

/*
 * Process the timers and logic upon receiving data from a source we are not tracking yet.
 *
 * [in,out] receiver Receiver for which this source data was received - new source is added to its
 *                   tree.
 * [in] sender_cid CID of the sACN source that sent this data.
 * [in] header Header data contained in the sACN packet.
 * [in] seq Sequence number of the sACN packet.
 * [out] source_limit_exceeded Notification data to deliver if a source limit exceeded
 * condition should be forwarded to the app.
 * [out] notify Whether or not to forward the data to the user in a notification.
 */
void process_new_source_data(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const SacnHeaderData* header,
                             uint8_t seq, SacnTrackedSource** new_source,
                             SourceLimitExceededNotification* source_limit_exceeded, bool* notify)
{
#if SACN_ETC_PRIORITY_EXTENSION
  if ((header->start_code != SACN_STARTCODE_DMX) && (header->start_code != SACN_STARTCODE_PRIORITY))
    return;
#else
  if (header->start_code != SACN_STARTCODE_DMX)
    return;
#endif

  // Notify universe data during and after the sampling period.
  *notify = true;

  // A new source has appeared!
  SacnTrackedSource* src = NULL;

  size_t current_number_of_sources = etcpal_rbtree_size(&receiver->sources);
#if SACN_DYNAMIC_MEM
  size_t max_number_of_sources = receiver->source_count_max;
  bool infinite_sources = (max_number_of_sources == SACN_RECEIVER_INFINITE_SOURCES);
#else
  size_t max_number_of_sources = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE;
  bool infinite_sources = false;
#endif

  if (infinite_sources || (current_number_of_sources < max_number_of_sources))
    src = ALLOC_TRACKED_SOURCE();

  if (!src)
  {
    // No room for new source.
    if (!receiver->suppress_limit_exceeded_notification)
    {
      receiver->suppress_limit_exceeded_notification = true;
      source_limit_exceeded->callback = receiver->callbacks.source_limit_exceeded;
      source_limit_exceeded->context = receiver->callbacks.context;
      source_limit_exceeded->handle = receiver->keys.handle;
      source_limit_exceeded->universe = receiver->keys.universe;
      return;
    }
    else
    {
      // Notification suppressed - don't notify
      return;
    }
  }
  src->cid = *sender_cid;
  ETCPAL_MSVC_NO_DEP_WRN strcpy(src->name, header->source_name);
  etcpal_timer_start(&src->packet_timer, SOURCE_LOSS_TIMEOUT);
  src->seq = seq;
  src->terminated = false;
  src->dmx_received_since_last_tick = true;

#if SACN_ETC_PRIORITY_EXTENSION
  // If we are in the sampling period, the wait period for PAP is not necessary.
  if (receiver->sampling)
  {
    if (header->start_code == SACN_STARTCODE_PRIORITY)
    {
      src->recv_state = kRecvStateWaitingForDmx;
      etcpal_timer_start(&src->pap_timer, SOURCE_LOSS_TIMEOUT);
    }
    else
    {
      src->recv_state = kRecvStateHaveDmxOnly;
    }
  }
  else
  {
    // Even if this is a priority packet, we want to make sure that DMX packets are also being
    // sent before notifying.
    if (header->start_code == SACN_STARTCODE_PRIORITY)
      src->recv_state = kRecvStateWaitingForDmx;
    else
      src->recv_state = kRecvStateWaitingForPap;
    *notify = false;
    etcpal_timer_start(&src->pap_timer, WAIT_FOR_PRIORITY);
  }
#endif

  etcpal_rbtree_insert(&receiver->sources, src);
  *new_source = src;

  if (SACN_CAN_LOG(ETCPAL_LOG_DEBUG))
  {
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    etcpal_uuid_to_string(sender_cid, cid_str);
    SACN_LOG_DEBUG("Tracking new source %s (%s) with initial start code 0x%02x", header->source_name, cid_str,
                   header->start_code);
  }
}

/*
 * Function that implements sACN's sequence numbering algorithm.
 *
 * [in] new_seq The sequence number accompanying the new packet.
 * [in] old_seq The most recent previous sequence number that was received.
 * Returns whether this packet is in sequence and should be processed.
 */
bool check_sequence(int8_t new_seq, int8_t old_seq)
{
  int8_t seqnum_cmp = new_seq - old_seq;
  return (seqnum_cmp > 0 || seqnum_cmp <= -20);
}

void deliver_receive_callbacks(const EtcPalSockAddr* from_addr, const EtcPalUuid* sender_cid,
                               const SacnHeaderData* header, SourceLimitExceededNotification* source_limit_exceeded,
                               SourcePapLostNotification* source_pap_lost, UniverseDataNotification* universe_data)
{
#if !SACN_LOGGING_ENABLED
  ETCPAL_UNUSED_ARG(header);
#endif

  if (source_limit_exceeded->handle != SACN_RECEIVER_INVALID)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(sender_cid, cid_str);
      SACN_LOG_WARNING(
          "No room to track new sACN source %s (%s) on universe %u. This message will only be logged once each "
          "time the maximum number of sources is exceeded.",
          header->source_name, cid_str, header->universe_id);
    }

    if (source_limit_exceeded->callback)
      source_limit_exceeded->callback(source_limit_exceeded->handle, source_limit_exceeded->universe,
                                      source_limit_exceeded->context);
  }

  if (source_pap_lost->handle != SACN_RECEIVER_INVALID && source_pap_lost->callback)
  {
    source_pap_lost->callback(source_pap_lost->handle, source_pap_lost->universe, &source_pap_lost->source,
                              source_pap_lost->context);
  }

  if (universe_data->handle != SACN_RECEIVER_INVALID && universe_data->callback)
  {
    bool is_sampling = false;  // TODO: Pass in actual is_sampling
    universe_data->callback(universe_data->handle, from_addr, &universe_data->header, universe_data->pdata, is_sampling,
                            universe_data->context);
  }
}

/**************************************************************************************************
 * Internal helpers for processing periodic timeout functionality
 *************************************************************************************************/

/*
 * Handle periodic sACN Receive timeout functionality.
 * [in] recv_thread_context Context data for this thread.
 */
void process_receivers(SacnRecvThreadContext* recv_thread_context)
{
  SamplingStartedNotification* sampling_started = NULL;
  size_t num_sampling_started = 0;
  SamplingEndedNotification* sampling_ended = NULL;
  size_t num_sampling_ended = 0;
  SourcesLostNotification* sources_lost = NULL;
  size_t num_sources_lost = 0;

  if (sacn_lock())
  {
    size_t num_receivers = recv_thread_context->num_receivers;

    sampling_started = get_sampling_started_buffer(recv_thread_context->thread_id, num_receivers);
    sampling_ended = get_sampling_ended_buffer(recv_thread_context->thread_id, num_receivers);
    sources_lost = get_sources_lost_buffer(recv_thread_context->thread_id, num_receivers);
    if (!sampling_started || !sampling_ended || !sources_lost)
    {
      sacn_unlock();
      SACN_LOG_ERR("Could not allocate memory to track state data for sACN receivers!");
      return;
    }

    for (SacnReceiver* receiver = recv_thread_context->receivers; receiver; receiver = receiver->next)
    {
      // Check the sample period
      if (!receiver->notified_sampling_started)
      {
        receiver->notified_sampling_started = true;
        sampling_started[num_sampling_started].callback = receiver->callbacks.sampling_period_started;
        sampling_started[num_sampling_started].context = receiver->callbacks.context;
        sampling_started[num_sampling_started].handle = receiver->keys.handle;
        sampling_started[num_sampling_started].universe = receiver->keys.universe;
        ++num_sampling_started;
      }

      if (receiver->sampling && etcpal_timer_is_expired(&receiver->sample_timer))
      {
        receiver->sampling = false;
        sampling_ended[num_sampling_ended].callback = receiver->callbacks.sampling_period_ended;
        sampling_ended[num_sampling_ended].context = receiver->callbacks.context;
        sampling_ended[num_sampling_ended].handle = receiver->keys.handle;
        sampling_ended[num_sampling_ended].universe = receiver->keys.universe;
        ++num_sampling_ended;
      }

      process_receiver_sources(recv_thread_context->thread_id, receiver, &sources_lost[num_sources_lost++]);
    }

    sacn_unlock();
  }

  PeriodicCallbacks periodic_callbacks;
  periodic_callbacks.sources_lost_arr = sources_lost;
  periodic_callbacks.num_sources_lost = num_sources_lost;
  periodic_callbacks.sampling_started_arr = sampling_started;
  periodic_callbacks.num_sampling_started = num_sampling_started;
  periodic_callbacks.sampling_ended_arr = sampling_ended;
  periodic_callbacks.num_sampling_ended = num_sampling_ended;

  deliver_periodic_callbacks(&periodic_callbacks);
}

void process_receiver_sources(sacn_thread_id_t thread_id, SacnReceiver* receiver, SourcesLostNotification* sources_lost)
{
  SacnSourceStatusLists* status_lists = get_status_lists(thread_id);
  SacnTrackedSource** to_erase = get_to_erase_buffer(thread_id, etcpal_rbtree_size(&receiver->sources));

  if (!status_lists || !to_erase)
  {
    SACN_LOG_ERR("Couldn't allocate memory to process sACN receiver for universe %u!", receiver->keys.universe);
    return;
  }
  size_t num_to_erase = 0;

  // And iterate through the sources on each universe
  EtcPalRbIter src_it;
  etcpal_rbiter_init(&src_it);
  SacnTrackedSource* src = etcpal_rbiter_first(&src_it, &receiver->sources);
  while (src)
  {
    if (!check_source_timeouts(src, status_lists))
    {
      to_erase[num_to_erase++] = src;
      if (SACN_CAN_LOG(ETCPAL_LOG_DEBUG))
      {
        char cid_str[ETCPAL_UUID_STRING_BYTES];
        etcpal_uuid_to_string(&src->cid, cid_str);
        SACN_LOG_DEBUG("Removing internally tracked source %s", cid_str);
      }
    }

    src = etcpal_rbiter_next(&src_it);
  }

  mark_sources_offline(status_lists->offline, status_lists->num_offline, status_lists->unknown,
                       status_lists->num_unknown, &receiver->term_sets, receiver_state.expired_wait);
  mark_sources_online(status_lists->online, status_lists->num_online, receiver->term_sets);
  get_expired_sources(&receiver->term_sets, sources_lost);

  for (size_t i = 0; i < num_to_erase; ++i)
  {
    etcpal_rbtree_remove(&receiver->sources, to_erase[i]);
    FREE_TRACKED_SOURCE(to_erase[i]);
  }

  if (sources_lost->num_lost_sources > 0)
  {
    sources_lost->callback = receiver->callbacks.sources_lost;
    sources_lost->context = receiver->callbacks.context;
    sources_lost->handle = receiver->keys.handle;
    sources_lost->universe = receiver->keys.universe;
    for (size_t i = 0; i < sources_lost->num_lost_sources; ++i)
    {
      etcpal_rbtree_remove_with_cb(&receiver->sources, &sources_lost->lost_sources[i].cid, source_tree_dealloc);
    }
  }
}

/*
 * Check the various packet timeouts of a given source and add it to status lists if necessary.
 *
 * [in,out] src Source to be checked - some state is updated.
 * [out] status_lists Set of status lists to which to add the source based on its state.
 * Returns false if the source timed out while in a waiting state and should be removed immediately.
 */
bool check_source_timeouts(SacnTrackedSource* src, SacnSourceStatusLists* status_lists)
{
  bool res = true;

#if SACN_ETC_PRIORITY_EXTENSION

  switch (src->recv_state)
  {
    case kRecvStateWaitingForDmx:
      if (etcpal_timer_is_expired(&src->pap_timer))
        res = false;
      break;
    case kRecvStateWaitingForPap:
      if (etcpal_timer_is_expired(&src->packet_timer))
        res = false;
      break;
    case kRecvStateHaveDmxOnly:
    case kRecvStateHaveDmxAndPap:
      update_source_status(src, status_lists);
      break;
    default:
      break;
  }

#else  // SACN_ETC_PRIORITY_EXTENSION

  update_source_status(src, status_lists);

#endif  // SACN_ETC_PRIORITY_EXTENSION

  return res;
}

void update_source_status(SacnTrackedSource* src, SacnSourceStatusLists* status_lists)
{
  if (etcpal_timer_is_expired(&src->packet_timer))
  {
    if (!add_offline_source(status_lists, &src->cid, src->name, src->terminated) && SACN_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char cid_str[ETCPAL_UUID_BYTES];
      etcpal_uuid_to_string(&src->cid, cid_str);
      SACN_LOG_ERR(
          "Couldn't allocate memory to add offline source %s to status list. This could be a bug or resource "
          "exhaustion issue.",
          cid_str);
    }
  }
  else if (src->dmx_received_since_last_tick)
  {
    if (!add_online_source(status_lists, &src->cid, src->name) && SACN_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char cid_str[ETCPAL_UUID_BYTES];
      etcpal_uuid_to_string(&src->cid, cid_str);
      SACN_LOG_ERR(
          "Couldn't allocate memory to add online source %s to status list. This could be a bug or resource "
          "exhaustion issue.",
          cid_str);
    }
    src->dmx_received_since_last_tick = false;
  }
  else
  {
    if (!add_unknown_source(status_lists, &src->cid, src->name) && SACN_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char cid_str[ETCPAL_UUID_BYTES];
      etcpal_uuid_to_string(&src->cid, cid_str);
      SACN_LOG_ERR(
          "Couldn't allocate memory to add undetermined source %s to status list. This could be a bug or resource "
          "exhaustion issue.",
          cid_str);
    }
  }
}

void deliver_periodic_callbacks(const PeriodicCallbacks* periodic_callbacks)
{
  for (const SamplingStartedNotification* notif = periodic_callbacks->sampling_started_arr;
       notif < periodic_callbacks->sampling_started_arr + periodic_callbacks->num_sampling_started; ++notif)
  {
    if (notif->callback)
      notif->callback(notif->handle, notif->universe, notif->context);
  }

  for (const SamplingEndedNotification* notif = periodic_callbacks->sampling_ended_arr;
       notif < periodic_callbacks->sampling_ended_arr + periodic_callbacks->num_sampling_ended; ++notif)
  {
    if (notif->callback)
      notif->callback(notif->handle, notif->universe, notif->context);
  }

  for (const SourcesLostNotification* notif = periodic_callbacks->sources_lost_arr;
       notif < periodic_callbacks->sources_lost_arr + periodic_callbacks->num_sources_lost; ++notif)
  {
    if (notif->callback)
      notif->callback(notif->handle, notif->universe, notif->lost_sources, notif->num_lost_sources, notif->context);
  }
}

/**************************************************************************************************
 * Internal helpers for managing the trees that track receivers and sources
 *************************************************************************************************/

int receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnReceiver* a = (const SacnReceiver*)value_a;
  const SacnReceiver* b = (const SacnReceiver*)value_b;
  return (a->keys.handle > b->keys.handle) - (a->keys.handle < b->keys.handle);
}

int receiver_compare_by_universe(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnReceiver* a = (const SacnReceiver*)value_a;
  const SacnReceiver* b = (const SacnReceiver*)value_b;
  return (a->keys.universe > b->keys.universe) - (a->keys.universe < b->keys.universe);
}

int tracked_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(tree);

  const SacnTrackedSource* a = (const SacnTrackedSource*)value_a;
  const SacnTrackedSource* b = (const SacnTrackedSource*)value_b;
  return ETCPAL_UUID_CMP(&a->cid, &b->cid);
}

EtcPalRbNode* node_alloc(void)
{
#if SACN_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(sacnrecv_rb_nodes);
#endif
}

void node_dealloc(EtcPalRbNode* node)
{
#if SACN_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(sacnrecv_rb_nodes, node);
#endif
}

/* Helper function for clearing an EtcPalRbTree containing sources. */
static void source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);
  FREE_TRACKED_SOURCE(node->value);
  node_dealloc(node);
}

/* Helper function for clearing an EtcPalRbTree containing SacnReceivers. */
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  SacnReceiver* receiver = (SacnReceiver*)node->value;
  etcpal_rbtree_clear_with_cb(&receiver->sources, source_tree_dealloc);
  if (receiver->ipv4_socket != ETCPAL_SOCKET_INVALID)
    sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv4_socket, true);
  if (receiver->ipv6_socket != ETCPAL_SOCKET_INVALID)
    sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv6_socket, true);
  FREE_RECEIVER(receiver);
  node_dealloc(node);
}

bool receiver_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  return (etcpal_rbtree_find(&receiver_state.receivers, &handle_val) != NULL);
}
