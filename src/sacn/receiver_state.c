/******************************************************************************
 * Copyright 2021 ETC Inc.
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

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "sacn/private/source_loss.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"
#include "sacn/private/sockets.h"
#include "sacn/private/receiver_state.h"
#include "sacn/private/source_detector_state.h"
#include "sacn/private/util.h"
#include "etcpal/acn_pdu.h"
#include "etcpal/acn_rlp.h"
#include "etcpal/netint.h"
#include "etcpal/pack.h"
#include "etcpal/timer.h"

/****************************** Private macros *******************************/

/***************************** Private constants *****************************/

static const EtcPalThreadParams kReceiverThreadParams = {SACN_RECEIVER_THREAD_PRIORITY, SACN_RECEIVER_THREAD_STACK,
                                                         "sACN Receive Thread", NULL};

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

static uint32_t expired_wait;

static IntHandleManager handle_mgr;

/*********************** Private function prototypes *************************/

// Receiver creation and destruction
static bool receiver_handle_in_use(int handle_val, void* cookie);

static etcpal_error_t start_receiver_thread(SacnRecvThreadContext* recv_thread_context);

static void sacn_receive_thread(void* arg);

// Receiving incoming data
static void handle_incoming(SacnRecvThreadContext* context, const uint8_t* data, size_t datalen,
                            const EtcPalSockAddr* from_addr);
static void handle_sacn_data_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                                    const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr);
static void handle_sacn_extended_packet(SacnRecvThreadContext* context, const uint8_t* data, size_t datalen,
                                        const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr);
static void process_null_start_code(const SacnReceiver* receiver, SacnTrackedSource* src,
                                    SourcePapLostNotification* source_pap_lost, bool* notify);
#if SACN_ETC_PRIORITY_EXTENSION
static void process_pap(const SacnReceiver* receiver, SacnTrackedSource* src, bool* notify);
#endif
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

/*************************** Function definitions ****************************/

etcpal_error_t sacn_receiver_state_init(void)
{
  init_int_handle_manager(&handle_mgr, receiver_handle_in_use, NULL);
  expired_wait = SACN_DEFAULT_EXPIRED_WAIT_MS;

  return kEtcPalErrOk;
}

void sacn_receiver_state_deinit(void)
{
  sacn_thread_id_t threads_ids_to_deinit[SACN_RECEIVER_MAX_THREADS];
  etcpal_thread_t* threads_handles_to_deinit[SACN_RECEIVER_MAX_THREADS];
  int num_threads_to_deinit = 0;

  // Stop all receive threads
  if (sacn_lock())
  {
    for (unsigned int i = 0; i < sacn_mem_get_num_threads(); ++i)
    {
      SacnRecvThreadContext* thread_context = get_recv_thread_context(i);
      if (thread_context && thread_context->running)
      {
        thread_context->running = false;
        threads_ids_to_deinit[num_threads_to_deinit] = thread_context->thread_id;
        threads_handles_to_deinit[num_threads_to_deinit] = &thread_context->thread_handle;
        ++num_threads_to_deinit;
      }
    }

    sacn_unlock();
  }

  for (int i = 0; i < num_threads_to_deinit; ++i)
    etcpal_thread_join(threads_handles_to_deinit[i]);

  if (sacn_lock())
  {
    for (int i = 0; i < num_threads_to_deinit; ++i)
    {
      SacnRecvThreadContext* thread_context = get_recv_thread_context(threads_ids_to_deinit[i]);
      if (thread_context)
        sacn_cleanup_dead_sockets(thread_context);
    }

    remove_all_receiver_sockets(kCloseSocketNow);

    sacn_unlock();
  }
}

sacn_receiver_t get_next_receiver_handle()
{
  return get_next_int_handle(&handle_mgr, -1);
}

size_t get_receiver_netints(const SacnReceiver* receiver, EtcPalMcastNetintId* netints, size_t netints_size)
{
  for (size_t i = 0; netints && (i < netints_size) && (i < receiver->netints.num_netints); ++i)
    netints[i] = receiver->netints.netints[i];

  return receiver->netints.num_netints;
}

void set_expired_wait(uint32_t wait_ms)
{
  expired_wait = wait_ms;
}

uint32_t get_expired_wait()
{
  return expired_wait;
}

etcpal_error_t clear_term_sets_and_sources(SacnReceiver* receiver)
{
  clear_term_set_list(receiver->term_sets);
  receiver->term_sets = NULL;
  return clear_receiver_sources(receiver);
}

/*
 * Pick a thread for the receiver based on current load balancing, create the receiver's sockets,
 * and assign it to that thread.
 *
 * [in,out] receiver Receiver instance to assign. Make sure the universe ID is up-to-date.
 * Returns error code indicating the result of the operations.
 */
etcpal_error_t assign_receiver_to_thread(SacnReceiver* receiver)
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

  etcpal_error_t res = add_receiver_sockets(receiver);

  if ((res == kEtcPalErrOk) && !assigned_thread->running)
  {
    res = start_receiver_thread(assigned_thread);
    if (res != kEtcPalErrOk)
    {
      remove_receiver_sockets(receiver, kCloseSocketNow);
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
 * Assign source detector to its thread and create the detector's sockets.
 *
 * [in,out] detector Source detector instance to assign.
 * Returns error code indicating the result of the operations.
 */
etcpal_error_t assign_source_detector_to_thread(SacnSourceDetector* detector)
{
  SACN_ASSERT(sacn_mem_get_num_threads() > 0);

  SacnRecvThreadContext* assigned_thread = get_recv_thread_context(0);
  detector->thread_id = 0;

  SACN_ASSERT(assigned_thread);

  etcpal_error_t res = add_source_detector_sockets(detector);

  if ((res == kEtcPalErrOk) && !assigned_thread->running)
  {
    res = start_receiver_thread(assigned_thread);
    if (res != kEtcPalErrOk)
      remove_source_detector_sockets(detector, kCloseSocketNow);
  }

  if (res == kEtcPalErrOk)
    assigned_thread->source_detector = detector;

  return res;
}

/*
 * Remove a receiver instance from a receiver thread. After this completes, the thread will no
 * longer process timeouts for that receiver.
 *
 * [in,out] receiver Receiver to remove.
 * [in] socket_close_behavior Whether to close the socket immediately (e.g. on full library shutdown).
 * Returns error code indicating the result of the removal operation.
 */
void remove_receiver_from_thread(SacnReceiver* receiver, socket_close_behavior_t socket_close_behavior)
{
  SacnRecvThreadContext* context = get_recv_thread_context(receiver->thread_id);
  if (context)
  {
    remove_receiver_sockets(receiver, socket_close_behavior);
    remove_receiver_from_list(context, receiver);
  }
}

/*
 * Remove a source detector instance from a receiver thread. After this completes, the thread will no
 * longer process the source detector.
 *
 * [in,out] detector Source detector to remove.
 * [in] socket_close_behavior Whether to close the socket immediately (e.g. on full library shutdown).
 * Returns error code indicating the result of the removal operation.
 */
void remove_source_detector_from_thread(SacnSourceDetector* detector, socket_close_behavior_t socket_close_behavior)
{
  SacnRecvThreadContext* context = get_recv_thread_context(detector->thread_id);
  if (context)
  {
    remove_source_detector_sockets(detector, socket_close_behavior);
    context->source_detector = NULL;
  }
}

/*
 * Initialize a receiver's IPv4 and IPv6 sockets. Make sure to take the sACN lock before calling.
 *
 * [in] receiver Receiver to add sockets for. Make sure the universe ID, thread ID, IP supported, and netints are
 * initialized.
 * Returns error code indicating the result of adding the sockets.
 */
etcpal_error_t add_receiver_sockets(SacnReceiver* receiver)
{
  etcpal_error_t ipv4_res = kEtcPalErrNoNetints;
  etcpal_error_t ipv6_res = kEtcPalErrNoNetints;

  if (supports_ipv4(receiver->ip_supported))
  {
    ipv4_res =
        sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV4, receiver->keys.universe,
                                 receiver->netints.netints, receiver->netints.num_netints, &receiver->ipv4_socket);
  }

  if (((ipv4_res == kEtcPalErrOk) || (ipv4_res == kEtcPalErrNoNetints)) && supports_ipv6(receiver->ip_supported))
  {
    ipv6_res =
        sacn_add_receiver_socket(receiver->thread_id, kEtcPalIpTypeV6, receiver->keys.universe,
                                 receiver->netints.netints, receiver->netints.num_netints, &receiver->ipv6_socket);
  }

  etcpal_error_t result =
      (((ipv4_res == kEtcPalErrNoNetints) || (ipv4_res == kEtcPalErrOk)) && (ipv6_res != kEtcPalErrNoNetints))
          ? ipv6_res
          : ipv4_res;

  if ((result != kEtcPalErrOk) && (ipv4_res == kEtcPalErrOk))
    sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv4_socket, kCloseSocketNow);

  return result;
}

etcpal_error_t add_source_detector_sockets(SacnSourceDetector* detector)
{
  etcpal_error_t ipv4_res = kEtcPalErrNoNetints;
  etcpal_error_t ipv6_res = kEtcPalErrNoNetints;

  if (supports_ipv4(detector->ip_supported))
  {
    ipv4_res =
        sacn_add_receiver_socket(detector->thread_id, kEtcPalIpTypeV4, SACN_DISCOVERY_UNIVERSE,
                                 detector->netints.netints, detector->netints.num_netints, &detector->ipv4_socket);
  }

  if (((ipv4_res == kEtcPalErrOk) || (ipv4_res == kEtcPalErrNoNetints)) && supports_ipv6(detector->ip_supported))
  {
    ipv6_res =
        sacn_add_receiver_socket(detector->thread_id, kEtcPalIpTypeV6, SACN_DISCOVERY_UNIVERSE,
                                 detector->netints.netints, detector->netints.num_netints, &detector->ipv6_socket);
  }

  etcpal_error_t result =
      (((ipv4_res == kEtcPalErrNoNetints) || (ipv4_res == kEtcPalErrOk)) && (ipv6_res != kEtcPalErrNoNetints))
          ? ipv6_res
          : ipv4_res;

  if ((result != kEtcPalErrOk) && (ipv4_res == kEtcPalErrOk))
    sacn_remove_receiver_socket(detector->thread_id, &detector->ipv4_socket, kCloseSocketNow);

  return result;
}

void begin_sampling_period(SacnReceiver* receiver)
{
  receiver->sampling = true;
  receiver->notified_sampling_started = false;
  etcpal_timer_start(&receiver->sample_timer, SACN_SAMPLE_TIME);
}

/*
 * Remove a receiver's sockets, choosing whether to close them now or wait until the next thread cycle.
 *
 * [in/out] receiver Receiver whose sockets to remove. Socket handles are set to invalid.
 * [in] close_behavior Whether to close the sockets now or wait until the next thread cycle.
 */
void remove_receiver_sockets(SacnReceiver* receiver, socket_close_behavior_t close_behavior)
{
  if (receiver->ipv4_socket != ETCPAL_SOCKET_INVALID)
    sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv4_socket, close_behavior);
  if (receiver->ipv6_socket != ETCPAL_SOCKET_INVALID)
    sacn_remove_receiver_socket(receiver->thread_id, &receiver->ipv6_socket, close_behavior);
}

/*
 * Remove a source detector's sockets, choosing whether to close them now or wait until the next thread cycle.
 *
 * [in/out] detector Source detector whose sockets to remove. Socket handles are set to invalid.
 * [in] close_behavior Whether to close the sockets now or wait until the next thread cycle.
 */
void remove_source_detector_sockets(SacnSourceDetector* detector, socket_close_behavior_t close_behavior)
{
  if (detector->ipv4_socket != ETCPAL_SOCKET_INVALID)
    sacn_remove_receiver_socket(detector->thread_id, &detector->ipv4_socket, close_behavior);
  if (detector->ipv6_socket != ETCPAL_SOCKET_INVALID)
    sacn_remove_receiver_socket(detector->thread_id, &detector->ipv6_socket, close_behavior);
}

/*
 * Remove all receiver sockets, choosing whether to close them now or wait until the next thread cycle.
 *
 * The sACN lock should be locked before calling this.
 *
 * [in] close_behavior Whether to close the sockets now or wait until the next thread cycle.
 */
void remove_all_receiver_sockets(socket_close_behavior_t close_behavior)
{
  EtcPalRbIter iter;
  for (SacnReceiver* receiver = get_first_receiver(&iter); receiver; receiver = get_next_receiver(&iter))
    remove_receiver_sockets(receiver, close_behavior);
}

/*
 * Called in a loop by each receiver thread to manage incoming data and state for receivers and/or the source detector.
 */
void read_network_and_process(SacnRecvThreadContext* context)
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
    handle_incoming(context, read_result.data, read_result.data_len, &read_result.from_addr);
  }
  else if (read_res != kEtcPalErrTimedOut)
  {
    if (read_res != kEtcPalErrNoSockets)
    {
      SACN_LOG_WARNING("Error occurred while attempting to read sACN incoming data: '%s'.", etcpal_strerror(read_res));
    }
    etcpal_thread_sleep(SACN_RECEIVER_READ_TIMEOUT_MS);
  }

  if (!context->periodic_timer_started)
  {
    etcpal_timer_start(&context->periodic_timer, SACN_PERIODIC_INTERVAL);
    context->periodic_timer_started = true;
  }

  if (etcpal_timer_is_expired(&context->periodic_timer))
  {
    process_receivers(context);
    process_source_detector(context);
    etcpal_timer_reset(&context->periodic_timer);
  }
}

/**************************************************************************************************
 * Helpers for receiver creation and destruction
 *************************************************************************************************/

bool receiver_handle_in_use(int handle_val, void* cookie)
{
  ETCPAL_UNUSED_ARG(cookie);

  SacnReceiver* tmp = NULL;
  return (lookup_receiver(handle_val, &tmp) == kEtcPalErrOk);
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
  recv_thread_context->periodic_timer_started = false;
  etcpal_error_t create_res = etcpal_thread_create(&recv_thread_context->thread_handle, &kReceiverThreadParams,
                                                   sacn_receive_thread, recv_thread_context);
  if (create_res != kEtcPalErrOk)
  {
    recv_thread_context->running = false;
  }
  return create_res;
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

  while (context->running)
    read_network_and_process(context);

  // Destroy the poll context
  etcpal_poll_context_deinit(&context->poll_context);
}

/**************************************************************************************************
 * Internal helpers for processing incoming sACN data
 *************************************************************************************************/

/*
 * Handle an incoming data packet on a receiver socket.
 *
 * [in] context Context for the thread in which the incoming data was received.
 * [in] data Incoming data buffer.
 * [in] datalen Size of data buffer.
 * [in] from_addr Network address from which the data was received.
 */
void handle_incoming(SacnRecvThreadContext* context, const uint8_t* data, size_t datalen,
                     const EtcPalSockAddr* from_addr)
{
  AcnUdpPreamble preamble;
  if (!acn_parse_udp_preamble(data, datalen, &preamble))
    return;

  AcnRootLayerPdu rlp;
  AcnPdu lpdu = ACN_PDU_INIT;
  while (acn_parse_root_layer_pdu(preamble.rlp_block, preamble.rlp_block_len, &rlp, &lpdu))
  {
    if (rlp.vector == ACN_VECTOR_ROOT_E131_DATA)
      handle_sacn_data_packet(context->thread_id, rlp.pdata, rlp.data_len, &rlp.sender_cid, from_addr);
    else if (rlp.vector == ACN_VECTOR_ROOT_E131_EXTENDED)
      handle_sacn_extended_packet(context, rlp.pdata, rlp.data_len, &rlp.sender_cid, from_addr);
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
 */
void handle_sacn_data_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                             const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr)
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

  parse_res = parse_sacn_data_packet(data, datalen, header, &seq, &is_termination_packet, &universe_data->pdata);

  if (!parse_res)
  {
    if (SACN_CAN_LOG(ETCPAL_LOG_WARNING))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(sender_cid, cid_str);
      SACN_LOG_WARNING("Ignoring malformed sACN data packet from component %s", cid_str);
    }
    return;
  }

  // Ignore SACN_STARTCODE_PRIORITY packets if SACN_ETC_PRIORITY_EXTENSION is disabled.
#if !SACN_ETC_PRIORITY_EXTENSION
  if (header->start_code == SACN_STARTCODE_PRIORITY)
    return;
#endif

  if (sacn_lock())
  {
    SacnReceiver* receiver = NULL;
    if (lookup_receiver_by_universe(header->universe_id, &receiver) != kEtcPalErrOk)
    {
      // We are not listening to this universe.
      sacn_unlock();
      return;
    }

    bool notify = false;
    header->source_handle = get_remote_source_handle(sender_cid);
    SacnTrackedSource* src = (SacnTrackedSource*)etcpal_rbtree_find(&receiver->sources, &header->source_handle);
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

      if (src)
        header->source_handle = src->handle;
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
        universe_data->receiver_handle = receiver->keys.handle;
        universe_data->universe = receiver->keys.universe;
        universe_data->is_sampling = receiver->sampling;
        universe_data->context = receiver->callbacks.context;
      }
    }

    sacn_unlock();
  }

  // Deliver callbacks if applicable.
  deliver_receive_callbacks(from_addr, sender_cid, header, source_limit_exceeded, source_pap_lost, universe_data);
}

/*
 * Handle an sACN Extended packet that has been unpacked from a Root Layer PDU.
 *
 * [in] context Context for the thread in which the extended packet was received.
 * [in] data Buffer containing the data packet.
 * [in] datalen Size of buffer.
 * [in] sender_cid CID from which the data was received.
 * [in] from_addr Network address from which the data was received.
 */
void handle_sacn_extended_packet(SacnRecvThreadContext* context, const uint8_t* data, size_t datalen,
                                 const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr)
{
  uint32_t vector;
  if (parse_framing_layer_vector(data, datalen, &vector))
  {
    if (vector == VECTOR_E131_EXTENDED_DISCOVERY)
    {
      size_t discovery_offset = (SACN_UNIVERSE_DISCOVERY_OFFSET - SACN_FRAMING_OFFSET);
      if (discovery_offset < datalen)
      {
        size_t discovery_len = (datalen - discovery_offset);
        size_t name_offset = (SACN_SOURCE_NAME_OFFSET - SACN_FRAMING_OFFSET);
        handle_sacn_universe_discovery_packet(context, &data[discovery_offset], discovery_len, sender_cid, from_addr,
                                              (char*)(&data[name_offset]));
      }
    }

    // TODO: sACN sync
  }
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
#if !SACN_ETC_PRIORITY_EXTENSION
  ETCPAL_UNUSED_ARG(receiver);
  ETCPAL_UNUSED_ARG(source_pap_lost);
#endif

  *notify = true;  // Notify universe data during and after the sampling period.

  // No matter how valid, we got something.
  src->dmx_received_since_last_tick = true;
  etcpal_timer_start(&src->packet_timer, SACN_SOURCE_LOSS_TIMEOUT);

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
        etcpal_timer_start(&src->pap_timer, SACN_SOURCE_LOSS_TIMEOUT);
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
        // In this case, also notify the source_pap_lost callback.
        source_pap_lost->callback = receiver->callbacks.source_pap_lost;
        source_pap_lost->source.handle = src->handle;
        source_pap_lost->source.cid = *(get_remote_source_cid(src->handle));
        ETCPAL_MSVC_NO_DEP_WRN strcpy(source_pap_lost->source.name, src->name);
        source_pap_lost->context = receiver->callbacks.context;
        source_pap_lost->handle = receiver->keys.handle;
        source_pap_lost->universe = receiver->keys.universe;

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
  ETCPAL_UNUSED_ARG(receiver);

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
      etcpal_timer_start(&src->pap_timer, SACN_SOURCE_LOSS_TIMEOUT);
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
  if (add_sacn_tracked_source(receiver, sender_cid, header->source_name, seq, header->start_code, new_source) ==
      kEtcPalErrOk)
  {
#if SACN_ETC_PRIORITY_EXTENSION
    if ((receiver->sampling && (header->start_code == SACN_STARTCODE_PRIORITY)) || !receiver->sampling)
      *notify = false;
#endif

    if (SACN_CAN_LOG(ETCPAL_LOG_DEBUG))
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(sender_cid, cid_str);
      SACN_LOG_DEBUG("Tracking new source %s (%s) with initial start code 0x%02x", header->source_name, cid_str,
                     header->start_code);
    }
  }
  else
  {
    // No room for new source.
    if (!receiver->suppress_limit_exceeded_notification)
    {
      receiver->suppress_limit_exceeded_notification = true;
      source_limit_exceeded->callback = receiver->callbacks.source_limit_exceeded;
      source_limit_exceeded->context = receiver->callbacks.context;
      source_limit_exceeded->handle = receiver->keys.handle;
      source_limit_exceeded->universe = receiver->keys.universe;
    }
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

  if (universe_data->receiver_handle != SACN_RECEIVER_INVALID && universe_data->callback)
  {
    universe_data->callback(universe_data->receiver_handle, from_addr, &universe_data->header, universe_data->pdata,
                            universe_data->is_sampling, universe_data->context);
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
        etcpal_uuid_to_string(get_remote_source_cid(src->handle), cid_str);
        SACN_LOG_DEBUG("Removing internally tracked source %s", cid_str);
      }
    }

    src = etcpal_rbiter_next(&src_it);
  }

  mark_sources_offline(status_lists->offline, status_lists->num_offline, status_lists->unknown,
                       status_lists->num_unknown, &receiver->term_sets, expired_wait);
  mark_sources_online(status_lists->online, status_lists->num_online, receiver->term_sets);
  get_expired_sources(&receiver->term_sets, sources_lost);

  for (size_t i = 0; i < num_to_erase; ++i)
    remove_receiver_source(receiver, to_erase[i]->handle);

  if (sources_lost->num_lost_sources > 0)
  {
    sources_lost->callback = receiver->callbacks.sources_lost;
    sources_lost->context = receiver->callbacks.context;
    sources_lost->handle = receiver->keys.handle;
    sources_lost->universe = receiver->keys.universe;

    for (size_t i = 0; i < sources_lost->num_lost_sources; ++i)
      remove_receiver_source(receiver, sources_lost->lost_sources[i].handle);

    receiver->suppress_limit_exceeded_notification = false;
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
    if (!add_offline_source(status_lists, src->handle, src->name, src->terminated) && SACN_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char cid_str[ETCPAL_UUID_BYTES];
      etcpal_uuid_to_string(get_remote_source_cid(src->handle), cid_str);
      SACN_LOG_ERR(
          "Couldn't allocate memory to add offline source %s to status list. This could be a bug or resource "
          "exhaustion issue.",
          cid_str);
    }
  }
  else if (src->dmx_received_since_last_tick)
  {
    if (!add_online_source(status_lists, src->handle, src->name) && SACN_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char cid_str[ETCPAL_UUID_BYTES];
      etcpal_uuid_to_string(get_remote_source_cid(src->handle), cid_str);
      SACN_LOG_ERR(
          "Couldn't allocate memory to add online source %s to status list. This could be a bug or resource "
          "exhaustion issue.",
          cid_str);
    }
    src->dmx_received_since_last_tick = false;
  }
  else
  {
    if (!add_unknown_source(status_lists, src->handle, src->name) && SACN_CAN_LOG(ETCPAL_LOG_ERR))
    {
      char cid_str[ETCPAL_UUID_BYTES];
      etcpal_uuid_to_string(get_remote_source_cid(src->handle), cid_str);
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
