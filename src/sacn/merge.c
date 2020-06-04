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

#include "sacn/merge.h"
#include "sacn/private/merge.h"

//TODO: CLEANUP
//#include <limits.h>
//#include <stdint.h>
//#include <string.h>
//#include "etcpal/acn_pdu.h"
//#include "etcpal/acn_rlp.h"
//#include "etcpal/common.h"
//#include "etcpal/rbtree.h"
//#include "sacn/private/common.h"
//#include "sacn/private/data_loss.h"
//#include "sacn/private/mem.h"
//#include "sacn/private/pdu.h"
//#include "sacn/private/receiver.h"
//#include "sacn/private/sockets.h"
//#include "sacn/private/util.h"

#if SACN_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private constants *****************************/
/****************************** Private macros *******************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */

/* TODO CLEANUP:
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
*/

/**************************** Private variables ******************************/

#if !SACN_DYNAMIC_MEM
/* TODO: CLEANUP:
ETCPAL_MEMPOOL_DEFINE(sacnrecv_receivers, SacnReceiver, SACN_RECEIVER_MAX_UNIVERSES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_tracked_sources, SacnTrackedSource, SACN_RECEIVER_TOTAL_MAX_SOURCES);
ETCPAL_MEMPOOL_DEFINE(sacnrecv_rb_nodes, EtcPalRbNode, SACN_RECEIVER_MAX_RB_NODES);
*/
#endif

/*********************** Private function prototypes *************************/

//TODO: Clean up
// Receiver creation and destruction
//static etcpal_error_t validate_receiver_config(const SacnReceiverConfig* config);
//static SacnReceiver* create_new_receiver(const SacnReceiverConfig* config);
//static void handle_sacn_data_packet(sacn_thread_id_t thread_id, const uint8_t* data, size_t datalen,
                                    //const EtcPalUuid* sender_cid, const EtcPalSockAddr* from_addr, bool draft);

/*************************** Function definitions ****************************/

/**************************************************************************************************
 * API functions
 *************************************************************************************************/

/* Initialize the sACN Receiver module. Internal function called from sacn_init(). */
etcpal_error_t dmx_merger_init(void)
{
  return kEtcPalNotImpl;

  //TODO: CLEANUP
  /*
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
  */
}

/* Deinitialize the sACN Receiver module. Internal function called from sacn_deinit(). */
void dmx_merger_deinit(void)
{
  /*TODO CLEANUP:
  // Clear out the rest of the state tracking
  etcpal_rbtree_clear_with_cb(&receiver_state.receivers, universe_tree_dealloc);
  etcpal_rbtree_clear(&receiver_state.receivers_by_universe);
  memset(&receiver_state, 0, sizeof receiver_state);
  */
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
etcpal_error_t dmx_merger_create_universe(const DmxMergerUniverseConfig* config, universe_handle_t* handle);
etcpal_error_t dmx_merger_destroy_universe(universe_handle_t handle);

etcpal_error_t dmx_merger_add_source(universe_handle_t universe, const EtcPalUuid* source_cid,
                                     source_id_t* source_handle);
etcpal_error_t dmx_merger_remove_source(universe_handle_t universe, source_id_t source);
const DmxMergerSource* dmx_merger_get_source(universe_handle_t universe, source_id_t source);
etcpal_error_t dmx_merger_update_source_data(universe_handle_t universe, source_id_t source, const uint8_t* new_values,
                                             size_t new_values_count, uint8_t priority,
                                             const uint8_t* address_priorities, size_t address_priorities_count);
//TODO: If Receiver API changes to notify both values and per-address priority data in the same callback, this should change!!
etcpal_error_t dmx_merger_update_source_from_sacn(universe_handle_t universe, source_id_t source,
                                                  const SacnHeaderData* header, const uint8_t* pdata);

//TODO: Do we need this?
etcpal_error_t dmx_merger_recalculate(universe_handle_t universe);
