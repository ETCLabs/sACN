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
#include "sacn/private/data_loss.h"
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

/***************************** Private constants *****************************/

static const EtcPalThreadParams kReceiverThreadParams = {SACN_RECEIVER_THREAD_PRIORITY, SACN_RECEIVER_THREAD_STACK,
                                                         "sACN Receive Thread", NULL};

#define SACN_PERIODIC_INTERVAL 120

/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if SACN_DYNAMIC_MEM
#define ALLOC_RECEIVER() malloc(sizeof(SacnReceiver))
#define ALLOC_TRACKED_SOURCE() malloc(sizeof(SacnTrackedSource))
#define FREE_RECEIVER(ptr) free(ptr)
#define FREE_TRACKED_SOURCE(ptr) free(ptr)
#else
#define ALLOC_RECEIVER() etcpal_mempool_alloc(sacnrecv_receivers)
#define ALLOC_TRACKED_SOURCE() etcpal_mempool_alloc(sacnrecv_tracked_sources)
#define FREE_RECEIVER(ptr) etcpal_mempool_free(sacnrecv_receivers, ptr)
#define FREE_TRACKED_SOURCE(ptr) etcpal_mempool_free(sacnrecv_tracked_sources, ptr)
#endif

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

static SacnReceiver* create_new_receiver(const SacnReceiverConfig* config);
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
                                    UniverseDataNotification* universe_data,
                                    SourcePcpLostNotification* source_pcp_lost);
static void process_pcp(const SacnReceiver* receiver, SacnTrackedSource* src, UniverseDataNotification* universe_data);
static void process_new_source_data(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const SacnHeaderData* header,
                                    uint8_t seq, UniverseDataNotification* universe_data,
                                    SourceLimitExceededNotification* source_limit_exceeded);
static bool check_sequence(int8_t new_seq, int8_t old_seq);
static void deliver_receive_callbacks(const EtcPalSockAddr* from_addr, const EtcPalUuid* sender_cid,
                                      const SacnHeaderData* header,
                                      SourceLimitExceededNotification* source_limit_exceeded,
                                      SourcePcpLostNotification* source_pcp_lost,
                                      UniverseDataNotification* universe_data);

// Process periodic timeout functionality
static void process_receivers(SacnRecvThreadContext* recv_thread_context);
static void process_receiver_sources(sacn_thread_id_t thread_id, SacnReceiver* receiver,
                                     SourcesLostNotification* sources_lost);
static bool check_source_timeouts(SacnTrackedSource* src, SacnSourceStatusLists* status_lists);
static void update_source_status(SacnTrackedSource* src, SacnSourceStatusLists* status_lists);
static void deliver_periodic_callbacks(const SourcesLostNotification* sources_lost_arr, size_t num_sources_lost,
                                       const SamplingEndedNotification* sampling_ended_arr, size_t num_sampling_ended);

// Tree node management
static int tracked_source_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static int receiver_compare_by_universe(const EtcPalRbTree* tree, const void* value_a, const void* value_b);
static EtcPalRbNode* node_alloc(void);
static void node_dealloc(EtcPalRbNode* node);
static void source_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static void universe_tree_dealloc(const EtcPalRbTree* self, EtcPalRbNode* node);
static bool receiver_handle_in_use(int handle_val);

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
    init_int_handle_manager(&receiver_state.handle_mgr, receiver_handle_in_use);
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

/*!
 * \brief Create a new sACN receiver to listen for sACN data on a universe.
 *
 * An sACN receiver can listen on one universe at a time, and each universe can only be listened to
 * by one receiver at at time. The receiver is initially considered to be in a "sampling period"
 * where all data on the universe is reported immediately via the universe_data() callback. Data
 * should be stored but not acted upon until receiving a sampling_ended() callback for this
 * receiver. This prevents level jumps as sources with different priorities are discovered.
 *
 * \param[in] config Configuration parameters for the sACN receiver to be created.
 * \param[out] handle Filled in on success with a handle to the sACN receiver.
 * \return #kEtcPalErrOk: Receiver created successful.
 * \return #kEtcPalErrInvalid: Invalid parameter provided.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrExists: A receiver already exists which is listening on the specified universe.
 * \return #kEtcPalErrNoMem: No room to allocate memory for this receiver.
 * \return #kEtcPalErrNoNetints: No network interfaces were found on the system.
 * \return #kEtcPalErrNotFound: A network interface ID given was not found on the system.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_create(const SacnReceiverConfig* config, sacn_receiver_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;

  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = sacn_validate_netint_config(config->netints, config->num_netints);
  if (res != kEtcPalErrOk)
    return res;

  if (SACN_LOCK())
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
    {
      receiver = create_new_receiver(config);
      if (!receiver)
        res = kEtcPalErrNoMem;
    }

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
    SACN_UNLOCK();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/*!
 * \brief Destroy an sACN receiver instance.
 *
 * Tears down the receiver and any sources currently being tracked on the receiver's universe.
 * Stops listening for sACN on that universe.
 *
 * \param[in] handle Handle to the receiver to destroy.
 * \return #kEtcPalErrOk: Receiver destroyed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_destroy(sacn_receiver_t handle)
{
  if (!sacn_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrOk;
  if (SACN_LOCK())
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
    SACN_UNLOCK();
  }
  else
  {
    res = kEtcPalErrSys;
  }
  return res;
}

/*!
 * \brief Change the universe on which an sACN receiver is listening.
 *
 * An sACN receiver can only listen on one universe at a time. After this call completes
 * successfully, the receiver is in a sampling period for the new universe where all data on the
 * universe is reported immediately via the universe_data() callback. Data should be stored but not
 * acted upon until receiving a sampling_ended() callback for this receiver. This prevents level
 * jumps as sources with different priorities are discovered.
 *
 * \param[in] handle Handle to the receiver for which to change the universe.
 * \param[in] new_universe_id New universe number that this receiver should listen to.
 * \return #kEtcPalErrOk: Universe changed successfully.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNotFound: Handle does not correspond to a valid receiver.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t sacn_receiver_change_universe(sacn_receiver_t handle, uint16_t new_universe)
{
  // TODO
  ETCPAL_UNUSED_ARG(handle);
  ETCPAL_UNUSED_ARG(new_universe);
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Set the current version of the sACN standard to which the module is listening.
 *
 * This is a global option across all listening receivers.
 *
 * \param[in] version Version of sACN to listen to.
 */
void sacn_set_standard_version(sacn_standard_version_t version)
{
  if (!sacn_initialized())
    return;

  if (SACN_LOCK())
  {
    receiver_state.version_listening = version;
    SACN_UNLOCK();
  }
}

/*!
 * \brief Get the current version of the sACN standard to which the module is listening.
 *
 * This is a global option across all listening receivers.
 *
 * \return Version of sACN to which the module is listening, or #kSacnStandardVersionNone if the module is
 *         not initialized.
 */
sacn_standard_version_t sacnrecv_get_standard_version()
{
  sacn_standard_version_t res = kSacnStandardVersionNone;

  if (!sacn_initialized())
    return res;

  if (SACN_LOCK())
  {
    res = receiver_state.version_listening;
    SACN_UNLOCK();
  }
  return res;
}

/*!
 * \brief Set the expired notification wait time.
 *
 * The library will wait at least this long after a data loss condition has been encountered before
 * sending a \ref SacnRecvCallbacks::sources_lost "sources_lost()" notification. However, the wait
 * may be longer due to the data loss algorithm (see \ref data_loss_behavior).
 *
 * \param[in] wait_ms Wait time in milliseconds.
 */
void sacnrecv_set_expired_wait(uint32_t wait_ms)
{
  if (!sacn_initialized())
    return;

  if (SACN_LOCK())
  {
    receiver_state.expired_wait = wait_ms;
    SACN_UNLOCK();
  }
}

/*!
 * \brief Get the current value of the expired notification wait time.
 *
 * The library will wait at least this long after a data loss condition has been encountered before
 * sending a \ref SacnRecvCallbacks::sources_lost "sources_lost()" notification. However, the wait
 * may be longer due to the data loss algorithm (see \ref data_loss_behavior).
 *
 * \return Wait time in milliseconds.
 */
uint32_t sacnrecv_get_expired_wait()
{
  uint32_t res = SACN_DEFAULT_EXPIRED_WAIT_MS;

  if (!sacn_initialized())
    return res;

  if (SACN_LOCK())
  {
    res = receiver_state.expired_wait;
    SACN_UNLOCK();
  }
  return res;
}

/**************************************************************************************************
 * Internal helpers for receiver creation and destruction
 *************************************************************************************************/

/*
 * Allocate a new receiver instances and do essential first initialization, in preparation for
 * creating the sockets and subscriptions.
 *
 * [in] config Receiver configuration data.
 * Returns the new initialized receiver instance, or NULL if out of memory.
 */
SacnReceiver* create_new_receiver(const SacnReceiverConfig* config)
{
  SACN_ASSERT(config);

  sacn_receiver_t new_handle = get_next_int_handle(&receiver_state.handle_mgr);
  if (new_handle == SACN_RECEIVER_INVALID)
    return NULL;

  SacnReceiver* receiver = ALLOC_RECEIVER();
  if (!receiver)
    return NULL;

  receiver->keys.handle = new_handle;
  receiver->keys.universe = config->universe_id;
  receiver->thread_id = SACN_THREAD_ID_INVALID;

  receiver->socket = ETCPAL_SOCKET_INVALID;

  receiver->sampling = true;
  etcpal_timer_start(&receiver->sample_timer, SAMPLE_TIME);
  receiver->suppress_limit_exceeded_notification = false;
  etcpal_rbtree_init(&receiver->sources, tracked_source_compare, node_alloc, node_dealloc);
  receiver->term_sets = NULL;

  receiver->filter_preview_data = ((config->flags & SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA) != 0);

  receiver->callbacks = config->callbacks;
  receiver->callback_context = config->callback_context;

  receiver->next = NULL;

  return receiver;
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

  // TODO IPv6
  etcpal_error_t res = sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV4, config->universe_id,
                                                config->netints, config->num_netints, &receiver->socket);
  if (res == kEtcPalErrOk && !assigned_thread->running)
  {
    res = start_receiver_thread(assigned_thread);
    if (res != kEtcPalErrOk)
      sacn_remove_receiver_socket(receiver->thread_id, receiver->socket, true);
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
 * Returns error code indicating the result of the thread start operation.
 */
void remove_receiver_from_thread(SacnReceiver* receiver, bool close_socket_now)
{
  SacnRecvThreadContext* context = get_recv_thread_context(receiver->thread_id);
  if (context)
  {
    sacn_remove_receiver_socket(receiver->thread_id, receiver->socket, close_socket_now);
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
    if (SACN_LOCK())
    {
      sacn_add_pending_sockets(context);
      sacn_cleanup_dead_sockets(context);
      SACN_UNLOCK();
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
      handle_sacn_data_packet(thread_id, rlp.pdata, rlp.datalen, &rlp.sender_cid, from_addr, false);
    }
    else if (rlp.vector == ACN_VECTOR_ROOT_DRAFT_E131_DATA &&
             (receiver_state.version_listening == kSacnStandardVersionDraft ||
              receiver_state.version_listening == kSacnStandardVersionAll))
    {
      handle_sacn_data_packet(thread_id, rlp.pdata, rlp.datalen, &rlp.sender_cid, from_addr, true);
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
  SourcePcpLostNotification* source_pcp_lost = get_source_pcp_lost(thread_id);
  if (!universe_data || !source_limit_exceeded || !source_pcp_lost)
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

  if (header->start_code != SACN_STARTCODE_DMX
#if SACN_ETC_PRIORITY_EXTENSION
      && header->start_code != SACN_STARTCODE_PRIORITY
#endif
  )
  {
    // Unknown START Code.
    return;
  }

  if (SACN_LOCK())
  {
    SacnReceiverKeys lookup_keys;
    lookup_keys.universe = header->universe_id;
    SacnReceiver* receiver = (SacnReceiver*)etcpal_rbtree_find(&receiver_state.receivers_by_universe, &lookup_keys);
    if (!receiver)
    {
      // We are not listening to this universe.
      SACN_UNLOCK();
      return;
    }

    if (header->preview && receiver->filter_preview_data)
    {
      // This universe is filtering preview data.
      SACN_UNLOCK();
      return;
    }

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
        SACN_UNLOCK();
        return;
      }

      if (!check_sequence(seq, src->seq))
      {
        // Drop the packet
        SACN_UNLOCK();
        return;
      }
      src->seq = seq;

      // Based on the start code, update the timers.
      if (header->start_code == SACN_STARTCODE_DMX)
      {
        process_null_start_code(receiver, src, universe_data, source_pcp_lost);
      }
#if SACN_ETC_PRIORITY_EXTENSION
      else if (header->start_code == SACN_STARTCODE_PRIORITY)
      {
        process_pcp(receiver, src, universe_data);
      }
#endif
    }
    else if (!is_termination_packet)
    {
      process_new_source_data(receiver, sender_cid, header, seq, universe_data, source_limit_exceeded);
    }
    // Else we weren't tracking this source before and it is a termination packet. Ignore.

    SACN_UNLOCK();
  }

  // Deliver callbacks if applicable.
  deliver_receive_callbacks(from_addr, sender_cid, header, source_limit_exceeded, source_pcp_lost, universe_data);
}

/*
 * Process the timers and logic upon receiving NULL START Code data from an existing source.
 *
 * [in] receiver Receiver on which this source data was received.
 * [in,out] src Existing source from which this data was received - state tracking is updated.
 * [out] universe_data Notification data to deliver if this NULL START code data should be
 *                     forwarded to the app.
 * [out] source_pcp_lost Notification data to deliver if a PCP lost condition should be forwarded
 *                       to the app.
 */
void process_null_start_code(const SacnReceiver* receiver, SacnTrackedSource* src,
                             UniverseDataNotification* universe_data, SourcePcpLostNotification* source_pcp_lost)
{
  bool notify = true;

  // No matter how valid, we got something.
  src->dmx_received_since_last_tick = true;
  etcpal_timer_start(&src->packet_timer, DATA_LOSS_TIMEOUT);

#if SACN_ETC_PRIORITY_EXTENSION
  switch (src->recv_state)
  {
    case kRecvStateWaitingForDmx:
      // We had previously received PCP, were waiting for DMX and got it.
      if (receiver->sampling)
      {
        // We are in the sample period - notify immediately.
        src->recv_state = kRecvStateHaveDmxAndPcp;
      }
      else
      {
        // Now we wait for one more PCP packet before notifying.
        src->recv_state = kRecvStateWaitingForPcp;
        notify = false;
      }
      break;
    case kRecvStateWaitingForPcp:
      if (etcpal_timer_is_expired(&src->pcp_timer))
      {
        // Our per-channel-priority waiting period has expired. Keep the timer going in case the
        // source starts sending PCP later.
        src->recv_state = kRecvStateHaveDmxOnly;
        etcpal_timer_start(&src->pcp_timer, DATA_LOSS_TIMEOUT);
      }
      else
      {
        // We've received a DMX packet during our per-channel-priority waiting period. Don't notify.
        notify = false;
      }
      break;
    case kRecvStateHaveDmxOnly:
      // More DMX, nothing to see here
      break;
    case kRecvStateHaveDmxAndPcp:
      if (etcpal_timer_is_expired(&src->pcp_timer))
      {
        // Source stopped sending PCP but is still sending DMX. In this case, also notify the
        // source_pcp_lost callback.
        source_pcp_lost->callback = receiver->callbacks.source_pcp_lost;
        source_pcp_lost->source.cid = src->cid;
        ETCPAL_MSVC_NO_DEP_WRN strcpy(source_pcp_lost->source.name, src->name);
        source_pcp_lost->context = receiver->callback_context;
        src->recv_state = kRecvStateHaveDmxOnly;
      }
      break;
    default:
      break;
  }
#endif

  if (notify)
  {
    universe_data->callback = receiver->callbacks.universe_data;
    universe_data->handle = receiver->keys.handle;
    universe_data->context = receiver->callback_context;
  }
}

#if SACN_ETC_PRIORITY_EXTENSION
/*
 * Process the timers and logic upon receiving per-channel priority data from an existing source.
 *
 * [in] receiver Receiver instance for which PCP data was received.
 * [in,out] src Existing source from which this PCP data was received - state tracking is updated.
 * [out] universe_data Notification data to deliver if this PCP data should be forwarded to the app.
 */
void process_pcp(const SacnReceiver* receiver, SacnTrackedSource* src, UniverseDataNotification* universe_data)
{
  bool notify = true;

  switch (src->recv_state)
  {
    case kRecvStateWaitingForDmx:
      // Still waiting for DMX - ignore PCP packets until we've seen at least one DMX packet.
      notify = false;
      etcpal_timer_reset(&src->pcp_timer);
      break;
    case kRecvStateWaitingForPcp:
    case kRecvStateHaveDmxOnly:
      src->recv_state = kRecvStateHaveDmxAndPcp;
      etcpal_timer_start(&src->pcp_timer, DATA_LOSS_TIMEOUT);
      break;
    case kRecvStateHaveDmxAndPcp:
      etcpal_timer_reset(&src->pcp_timer);
      break;
    default:
      break;
  }

  if (notify)
  {
    universe_data->callback = receiver->callbacks.universe_data;
    universe_data->handle = receiver->keys.handle;
    universe_data->context = receiver->callback_context;
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
 * [out] universe_data Notification data to deliver if this data should be forwarded to the app.
 * [out] source_limit_exceeded Notification data to deliver if a source limit exceeded condition
 *                             should be forwarded to the app.
 */
void process_new_source_data(SacnReceiver* receiver, const EtcPalUuid* sender_cid, const SacnHeaderData* header,
                             uint8_t seq, UniverseDataNotification* universe_data,
                             SourceLimitExceededNotification* source_limit_exceeded)
{
  bool notify = true;

  // A new source has appeared!
  SacnTrackedSource* src = ALLOC_TRACKED_SOURCE();
  if (!src)
  {
    // No room for new source.
    if (!receiver->suppress_limit_exceeded_notification)
    {
      receiver->suppress_limit_exceeded_notification = true;
      source_limit_exceeded->callback = receiver->callbacks.source_limit_exceeded;
      source_limit_exceeded->context = receiver->callback_context;
      source_limit_exceeded->handle = receiver->keys.handle;
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
  etcpal_timer_start(&src->packet_timer, DATA_LOSS_TIMEOUT);
  src->seq = seq;
  src->terminated = false;
  src->dmx_received_since_last_tick = true;

#if SACN_ETC_PRIORITY_EXTENSION
  // If we are in the sampling period, the wait period for PCP is not necessary.
  if (receiver->sampling)
  {
    if (header->start_code == SACN_STARTCODE_PRIORITY)
    {
      src->recv_state = kRecvStateWaitingForDmx;
      etcpal_timer_start(&src->pcp_timer, DATA_LOSS_TIMEOUT);
      // Don't notify - wait for first DMX packet.
      notify = false;
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
      src->recv_state = kRecvStateWaitingForPcp;
    notify = false;
    etcpal_timer_start(&src->pcp_timer, WAIT_FOR_PRIORITY);
  }
#endif

  etcpal_rbtree_insert(&receiver->sources, src);

  if (notify)
  {
    universe_data->callback = receiver->callbacks.universe_data;
    universe_data->handle = receiver->keys.handle;
    universe_data->context = receiver->callback_context;
  }

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
                               SourcePcpLostNotification* source_pcp_lost, UniverseDataNotification* universe_data)
{
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
      source_limit_exceeded->callback(source_limit_exceeded->handle, source_limit_exceeded->context);
  }

  if (source_pcp_lost->handle != SACN_RECEIVER_INVALID && source_pcp_lost->callback)
  {
    source_pcp_lost->callback(source_pcp_lost->handle, &source_pcp_lost->source, source_pcp_lost->context);
  }

  if (universe_data->handle != SACN_RECEIVER_INVALID && universe_data->callback)
  {
    universe_data->callback(universe_data->handle, from_addr, &universe_data->header, universe_data->pdata,
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
  SamplingEndedNotification* sampling_ended = NULL;
  size_t num_sampling_ended = 0;
  SourcesLostNotification* sources_lost = NULL;
  size_t num_sources_lost = 0;

  if (SACN_LOCK())
  {
    size_t num_receivers = recv_thread_context->num_receivers;

    sampling_ended = get_sampling_ended_buffer(recv_thread_context->thread_id, num_receivers);
    sources_lost = get_sources_lost_buffer(recv_thread_context->thread_id, num_receivers);
    if (!sampling_ended || !sources_lost)
    {
      SACN_UNLOCK();
      SACN_LOG_ERR("Could not allocate memory to track state data for sACN receivers!");
      return;
    }

    for (SacnReceiver* receiver = recv_thread_context->receivers; receiver; receiver = receiver->next)
    {
      // Check the sample period
      if (receiver->sampling && etcpal_timer_is_expired(&receiver->sample_timer))
      {
        receiver->sampling = false;
        sampling_ended[num_sampling_ended].callback = receiver->callbacks.sampling_ended;
        sampling_ended[num_sampling_ended].context = receiver->callback_context;
        sampling_ended[num_sampling_ended].handle = receiver->keys.handle;
        ++num_sampling_ended;
      }

      process_receiver_sources(recv_thread_context->thread_id, receiver, &sources_lost[num_sources_lost++]);
    }

    SACN_UNLOCK();
  }

  deliver_periodic_callbacks(sources_lost, num_sources_lost, sampling_ended, num_sampling_ended);
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
    sources_lost->context = receiver->callback_context;
    sources_lost->handle = receiver->keys.handle;
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
      if (etcpal_timer_is_expired(&src->pcp_timer))
        res = false;
      break;
    case kRecvStateWaitingForPcp:
      if (etcpal_timer_is_expired(&src->packet_timer))
        res = false;
      break;
    case kRecvStateHaveDmxOnly:
    case kRecvStateHaveDmxAndPcp:
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
          "Couldn't allocate memory to add online source %s to status list. This could be a bug or resource "
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

void deliver_periodic_callbacks(const SourcesLostNotification* sources_lost_arr, size_t num_sources_lost,
                                const SamplingEndedNotification* sampling_ended_arr, size_t num_sampling_ended)
{
  for (const SamplingEndedNotification* notif = sampling_ended_arr; notif < sampling_ended_arr + num_sampling_ended;
       ++notif)
  {
    if (notif->callback)
      notif->callback(notif->handle, notif->context);
  }
  for (const SourcesLostNotification* notif = sources_lost_arr; notif < sources_lost_arr + num_sources_lost; ++notif)
  {
    if (notif->callback)
      notif->callback(notif->handle, notif->lost_sources, notif->num_lost_sources, notif->context);
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
  sacn_remove_receiver_socket(receiver->thread_id, receiver->socket, true);
  FREE_RECEIVER(receiver);
  node_dealloc(node);
}

bool receiver_handle_in_use(int handle_val)
{
  return (etcpal_rbtree_find(&receiver_state.receivers, &handle_val) != NULL);
}
