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

#include "sacn/private/receiver_state.h"

#include <limits>
#include <optional>
#include "etcpal/cpp/inet.h"
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/thread.h"
#include "etcpal_mock/timer.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiverState TestReceiverStateDynamic
#define TestReceiverThread TestReceiverThreadDynamic
#else
#define TestReceiverState TestReceiverStateStatic
#define TestReceiverThread TestReceiverThreadStatic
#endif

FAKE_VOID_FUNC(universe_data, sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*, bool,
               void*);
FAKE_VOID_FUNC(sources_lost, sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*);
FAKE_VOID_FUNC(sampling_period_started, sacn_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(sampling_period_ended, sacn_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(source_pap_lost, sacn_receiver_t, uint16_t, const SacnRemoteSource*, void*);
FAKE_VOID_FUNC(source_limit_exceeded, sacn_receiver_t, uint16_t, void*);

static const uint16_t kTestUniverse = 123u;

static const SacnReceiverCallbacks kTestCallbacks = {universe_data,         sources_lost,    sampling_period_started,
                                                     sampling_period_ended, source_pap_lost, source_limit_exceeded};
static const SacnReceiverConfig kTestReceiverConfig = {kTestUniverse, kTestCallbacks, SACN_RECEIVER_INFINITE_SOURCES,
                                                       0u, kSacnIpV4AndIpV6};

static const EtcPalUuid kTestCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22").get();
static const char kTestName[] = "Test Name";
static const EtcPalSockAddr kTestSockAddr = {SACN_PORT, etcpal::IpAddr::FromString("10.101.1.1").get()};

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};

static std::vector<SacnMcastInterface> kTestNetints = {{{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
                                                       {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}};

static const sacn_receiver_t kFirstReceiverHandle = 0;
static const uint8_t kTestPriority = 77u;
static const bool kTestPreview = false;

static const etcpal_socket_t kTestSocket = static_cast<etcpal_socket_t>(7);
static TerminationSet kTestTermSet = {{0u, 0u}, {nullptr, nullptr, 0u, nullptr, nullptr, nullptr}, nullptr};

static uint8_t test_data[SACN_MTU];
static uint8_t seq_num;
static unsigned int test_iteration;

class TestReceiverState : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    sacn_initialize_receiver_netints_fake.custom_fake = [](SacnInternalNetintArray* receiver_netints,
                                                           SacnMcastInterface* app_netints, size_t num_app_netints) {
#if SACN_DYNAMIC_MEM
      receiver_netints->netints = (EtcPalMcastNetintId*)calloc(num_app_netints, sizeof(EtcPalMcastNetintId));
#endif
      receiver_netints->num_netints = num_app_netints;

      for (size_t i = 0; i < num_app_netints; ++i)
      {
        receiver_netints->netints[i] = app_netints[i].iface;
        app_netints[i].status = kEtcPalErrOk;
      }

      return kEtcPalErrOk;
    };

    test_iteration = 1u;

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_state_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_socket_t*, socket_close_behavior_t) {};

    sacn_receiver_state_deinit();
    sacn_mem_deinit();
  }

  SacnReceiver* AddReceiver(uint16_t universe_id = kTestUniverse,
                            const SacnReceiverCallbacks& callbacks = kTestCallbacks,
                            sacn_ip_support_t ip_supported = kSacnIpV4AndIpV6)
  {
    SacnReceiverConfig config = kTestReceiverConfig;
    config.universe_id = universe_id;
    config.callbacks = callbacks;
    config.ip_supported = ip_supported;

    return AddReceiver(config);
  }

  SacnReceiver* AddReceiver(const SacnReceiverConfig& config)
  {
    SacnReceiver* receiver = nullptr;
    EXPECT_EQ(add_sacn_receiver(next_receiver_handle_++, &config, kTestNetints.data(), kTestNetints.size(), &receiver),
              kEtcPalErrOk);

    return receiver;
  }

  SacnTrackedSource* AddTrackedSource(SacnReceiver* receiver)
  {
    SacnTrackedSource* source = nullptr;
    EXPECT_EQ(add_sacn_tracked_source(receiver, &next_source_cid_, kTestName, 0u, 0u, &source), kEtcPalErrOk);

    ++next_source_cid_.data[0];

    return source;
  }

  sacn_receiver_t next_receiver_handle_ = kFirstReceiverHandle;
  EtcPalUuid next_source_cid_ = kTestCid;
};

class TestReceiverThread : public TestReceiverState
{
protected:
  void SetUp() override
  {
    TestReceiverState::SetUp();

    RESET_FAKE(universe_data);
    RESET_FAKE(sources_lost);
    RESET_FAKE(sampling_period_started);
    RESET_FAKE(sampling_period_ended);
    RESET_FAKE(source_pap_lost);
    RESET_FAKE(source_limit_exceeded);

    sacn_read_fake.return_val = kEtcPalErrTimedOut;

    test_receiver_ = AddReceiver();
    ASSERT_NE(test_receiver_, nullptr);

    begin_sampling_period(test_receiver_);
    ASSERT_EQ(assign_receiver_to_thread(test_receiver_), kEtcPalErrOk);

    seq_num = 0u;

    memset(test_data, 0, SACN_MTU);
  }

  void TearDown() override
  {
    if (test_receiver_)
    {
      remove_receiver_from_thread(test_receiver_, kCloseSocketNow);
      remove_sacn_receiver(test_receiver_);
      test_receiver_ = nullptr;
    }

    TestReceiverState::TearDown();
  }

  void InitTestData(uint8_t start_code, uint16_t universe, const uint8_t* data = nullptr, size_t data_len = 0u,
                    uint8_t flags = 0u, uint8_t sequence_number = seq_num)
  {
    init_sacn_data_send_buf(test_data, start_code, &kTestCid, kTestName, kTestPriority, universe, 0u, kTestPreview);

    if (data)
      update_send_buf_data(test_data, data, static_cast<uint16_t>(data_len), kDisableForceSync);

    test_data[SACN_OPTS_OFFSET] |= flags;
    test_data[SACN_SEQ_OFFSET] = sequence_number;

    sacn_read_fake.custom_fake = [](SacnRecvThreadContext*, SacnReadResult* read_result) {
      read_result->from_addr = kTestSockAddr;
      read_result->data = test_data;
      read_result->data_len = SACN_MTU;
      return kEtcPalErrOk;
    };
  }

  void UpdateTestReceiverConfig(const SacnReceiverConfig& config)
  {
    sacn_receiver_t handle = test_receiver_->keys.handle;
    remove_receiver_from_thread(test_receiver_, kQueueSocketForClose);
    remove_sacn_receiver(test_receiver_);
    EXPECT_EQ(add_sacn_receiver(handle, &config, kTestNetints.data(), kTestNetints.size(), &test_receiver_),
              kEtcPalErrOk);
    begin_sampling_period(test_receiver_);
    EXPECT_EQ(assign_receiver_to_thread(test_receiver_), kEtcPalErrOk);
  }

  void RunThreadCycle()
  {
    read_network_and_process_receivers(get_recv_thread_context(0u));
    ++seq_num;
    test_data[SACN_SEQ_OFFSET] = seq_num;
  }

  SacnReceiver* test_receiver_;
};

TEST_F(TestReceiverState, ExpiredWaitInitializes)
{
  EXPECT_EQ(get_expired_wait(), SACN_DEFAULT_EXPIRED_WAIT_MS);
}

TEST_F(TestReceiverState, InitializedThreadDeinitializes)
{
  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);
  EXPECT_EQ(sacn_cleanup_dead_sockets_fake.call_count, 0u);

  etcpal_thread_join_fake.custom_fake = [](etcpal_thread_t* id) {
    EXPECT_EQ(id, &get_recv_thread_context(0)->thread_handle);
    return kEtcPalErrOk;
  };
  sacn_cleanup_dead_sockets_fake.custom_fake = [](SacnRecvThreadContext* recv_thread_context) {
    EXPECT_EQ(recv_thread_context, get_recv_thread_context(0));
  };

  SacnReceiver* receiver = AddReceiver(kTestUniverse);
  assign_receiver_to_thread(receiver);

  EXPECT_EQ(get_recv_thread_context(0)->running, true);

  sacn_receiver_state_deinit();

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 1u);
  EXPECT_EQ(sacn_cleanup_dead_sockets_fake.call_count, 1u);

  EXPECT_EQ(get_recv_thread_context(0)->running, false);
}

TEST_F(TestReceiverState, UninitializedThreadDoesNotDeinitialize)
{
  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);
  EXPECT_EQ(sacn_cleanup_dead_sockets_fake.call_count, 0u);

  sacn_receiver_state_deinit();

  EXPECT_EQ(etcpal_thread_join_fake.call_count, 0u);
  EXPECT_EQ(sacn_cleanup_dead_sockets_fake.call_count, 0u);
}

TEST_F(TestReceiverState, DeinitRemovesAllReceiverSockets)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  assign_receiver_to_thread(AddReceiver(kTestUniverse));

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_socket_t*,
                                                    socket_close_behavior_t close_behavior) {
    EXPECT_EQ(close_behavior, kCloseSocketNow);
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_receiver_state_deinit();

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
}

TEST_F(TestReceiverState, GetNextReceiverHandleWorks)
{
  for (sacn_receiver_t handle = 0; handle < 10; ++handle)
    EXPECT_EQ(get_next_receiver_handle(), handle);
}

TEST_F(TestReceiverState, GetReceiverNetintsWorks)
{
  SacnReceiver* receiver = AddReceiver(kTestUniverse);

  std::vector<EtcPalMcastNetintId> netints(kTestNetints.size() * 2u);
  for (size_t i = 0u; i < (kTestNetints.size() * 2u); ++i)
  {
    netints[i].index = 0u;
    netints[i].ip_type = kEtcPalIpTypeInvalid;
  }

  size_t num_netints = get_receiver_netints(receiver, netints.data(), 1u);
  EXPECT_EQ(num_netints, kTestNetints.size());

  EXPECT_EQ(netints[0].index, kTestNetints[0].iface.index);
  EXPECT_EQ(netints[0].ip_type, kTestNetints[0].iface.ip_type);
  for (size_t i = 1u; i < netints.size(); ++i)
  {
    EXPECT_EQ(netints[i].index, 0u);
    EXPECT_EQ(netints[i].ip_type, kEtcPalIpTypeInvalid);
  }

  num_netints = get_receiver_netints(receiver, netints.data(), netints.size());
  EXPECT_EQ(num_netints, kTestNetints.size());

  for (size_t i = 0u; i < kTestNetints.size(); ++i)
  {
    EXPECT_EQ(netints[i].index, kTestNetints[i].iface.index);
    EXPECT_EQ(netints[i].ip_type, kTestNetints[i].iface.ip_type);
  }

  for (size_t i = kTestNetints.size(); i < netints.size(); ++i)
  {
    EXPECT_EQ(netints[i].index, 0u);
    EXPECT_EQ(netints[i].ip_type, kEtcPalIpTypeInvalid);
  }
}

TEST_F(TestReceiverState, GetSetExpiredWaitWorks)
{
  set_expired_wait(1u);
  EXPECT_EQ(get_expired_wait(), 1u);
  set_expired_wait(10u);
  EXPECT_EQ(get_expired_wait(), 10u);
  set_expired_wait(100u);
  EXPECT_EQ(get_expired_wait(), 100u);
}

TEST_F(TestReceiverState, ClearTermSetsAndSourcesWorks)
{
  SacnReceiver* receiver = AddReceiver();
  AddTrackedSource(receiver);

  EXPECT_EQ(etcpal_rbtree_size(&receiver->sources), 1u);
  EXPECT_EQ(clear_term_set_list_fake.call_count, 0u);

  clear_term_set_list_fake.custom_fake = [](TerminationSet* list) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);
    EXPECT_EQ(list, state->term_sets);
  };

  receiver->term_sets = &kTestTermSet;

  clear_term_sets_and_sources(receiver);

  EXPECT_EQ(etcpal_rbtree_size(&receiver->sources), 0u);
  EXPECT_EQ(clear_term_set_list_fake.call_count, 1u);
  EXPECT_EQ(receiver->term_sets, nullptr);
}

TEST_F(TestReceiverState, AssignReceiverToThreadWorks)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t universe,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    EXPECT_EQ(universe, kTestUniverse);
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };
  etcpal_thread_create_fake.custom_fake = [](etcpal_thread_t* id, const EtcPalThreadParams* params,
                                             void (*thread_fn)(void*), void* thread_arg) {
    ETCPAL_UNUSED_ARG(thread_fn);
    EXPECT_EQ(id, &get_recv_thread_context(0)->thread_handle);
    EXPECT_EQ(params->priority, static_cast<unsigned int>(SACN_RECEIVER_THREAD_PRIORITY));
    EXPECT_EQ(params->stack_size, static_cast<unsigned int>(SACN_RECEIVER_THREAD_STACK));
    EXPECT_EQ(strcmp(params->thread_name, "sACN Receive Thread"), 0);
    EXPECT_EQ(params->platform_data, nullptr);
    EXPECT_NE(thread_fn, nullptr);
    EXPECT_EQ(thread_arg, get_recv_thread_context(0));
    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver();

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);

  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrOk);

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 1u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(get_recv_thread_context(0)->running, true);
  EXPECT_EQ(get_recv_thread_context(0)->receivers->keys.universe, kTestUniverse);
}

TEST_F(TestReceiverState, AssignReceiverToThreadHandlesSocketError)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrNoNetints;
  };

  SacnReceiver* receiver = AddReceiver();

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);

  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrNoNetints);

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(get_recv_thread_context(0)->running, false);
  EXPECT_EQ(get_recv_thread_context(0)->receivers, nullptr);
}

TEST_F(TestReceiverState, AssignReceiverToThreadHandlesThreadError)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  etcpal_thread_create_fake.return_val = kEtcPalErrSys;

  SacnReceiver* receiver = AddReceiver();

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);

  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrSys);

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 1u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(get_recv_thread_context(0)->running, false);
  EXPECT_EQ(get_recv_thread_context(0)->receivers, nullptr);
}

TEST_F(TestReceiverState, RemoveReceiverFromThreadWorks)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver();
  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrOk);

  remove_receiver_from_thread(receiver, kQueueSocketForClose);

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(get_recv_thread_context(0)->receivers, nullptr);
}

TEST_F(TestReceiverState, AddReceiverSocketsWorks)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t universe,
                                                 const EtcPalMcastNetintId* netints, size_t num_netints,
                                                 etcpal_socket_t* socket) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, state->thread_id);
    EXPECT_TRUE((ip_type == kEtcPalIpTypeV4) || (ip_type == kEtcPalIpTypeV6));
    EXPECT_EQ(universe, kTestUniverse);

    for (size_t i = 0u; i < kTestNetints.size(); ++i)
    {
      EXPECT_EQ(netints[i].index, kTestNetints[i].iface.index);
      EXPECT_EQ(netints[i].ip_type, kTestNetints[i].iface.ip_type);
    }

    EXPECT_EQ(num_netints, kTestNetints.size());
    EXPECT_TRUE((socket == &state->ipv4_socket) || (socket == &state->ipv6_socket));

    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver();
  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrOk);  // Calls add_receiver_sockets
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
}

TEST_F(TestReceiverState, AddReceiverSocketsHandlesIpv4Only)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(ip_type, kEtcPalIpTypeV4);
    EXPECT_EQ(socket, &state->ipv4_socket);

    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4Only);
  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrOk);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 1u);
}

TEST_F(TestReceiverState, AddReceiverSocketsHandlesIpv6Only)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(ip_type, kEtcPalIpTypeV6);
    EXPECT_EQ(socket, &state->ipv6_socket);

    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV6Only);
  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrOk);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 1u);
}

TEST_F(TestReceiverState, AddReceiverSocketsHandlesErrors)
{
  SacnReceiver* receiver = AddReceiver();

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t,
                                                 etcpal_socket_t*) { return kEtcPalErrNoNetints; };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrNoNetints);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrNoNetints : kEtcPalErrOk;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrOk);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 4u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrNoNetints : kEtcPalErrSys;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrSys);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 6u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t,
                                                 etcpal_socket_t*) { return kEtcPalErrOk; };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrOk);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 8u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrOk : kEtcPalErrSys;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrSys);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 10u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrOk : kEtcPalErrNoNetints;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrOk);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 12u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t,
                                                 etcpal_socket_t*) { return kEtcPalErrSys; };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrSys);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 13u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrSys : kEtcPalErrNoNetints;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrSys);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 14u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrSys : kEtcPalErrOk;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrSys);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 15u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);
}

TEST_F(TestReceiverState, BeginSamplingPeriodWorks)
{
  SacnReceiver* receiver = AddReceiver();

  EXPECT_EQ(receiver->sampling, false);
  EXPECT_EQ(receiver->notified_sampling_started, false);

  begin_sampling_period(receiver);

  EXPECT_EQ(receiver->sampling, true);
  EXPECT_EQ(receiver->notified_sampling_started, false);
  EXPECT_EQ(receiver->sample_timer.interval, static_cast<uint32_t>(SACN_SAMPLE_TIME));
}

TEST_F(TestReceiverState, RemoveReceiverSocketsRemovesIpv4AndIpv6)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver();
  assign_receiver_to_thread(receiver);

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket,
                                                    socket_close_behavior_t close_behavior) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, 0u);
    EXPECT_TRUE((socket == &state->ipv4_socket) || (socket == &state->ipv6_socket));
    EXPECT_EQ(close_behavior, kCloseSocketNow);

    *socket = ETCPAL_SOCKET_INVALID;
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_receiver_sockets(receiver, kCloseSocketNow);

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
}

TEST_F(TestReceiverState, RemoveReceiverSocketsRemovesOnlyIpv4)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4Only);
  assign_receiver_to_thread(receiver);

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket,
                                                    socket_close_behavior_t close_behavior) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, 0u);
    EXPECT_EQ(socket, &state->ipv4_socket);
    EXPECT_EQ(close_behavior, kCloseSocketNow);

    *socket = ETCPAL_SOCKET_INVALID;
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_receiver_sockets(receiver, kCloseSocketNow);

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);
}

TEST_F(TestReceiverState, RemoveReceiverSocketsRemovesOnlyIpv6)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  SacnReceiver* receiver = AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV6Only);
  assign_receiver_to_thread(receiver);

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket,
                                                    socket_close_behavior_t close_behavior) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, 0u);
    EXPECT_EQ(socket, &state->ipv6_socket);
    EXPECT_EQ(close_behavior, kCloseSocketNow);

    *socket = ETCPAL_SOCKET_INVALID;
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_receiver_sockets(receiver, kCloseSocketNow);

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);
}

TEST_F(TestReceiverState, RemoveAllReceiverSocketsWorks)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    *socket = kTestSocket;
    return kEtcPalErrOk;
  };

  for (uint16_t i = 0u; i < SACN_RECEIVER_MAX_UNIVERSES; ++i)
    assign_receiver_to_thread(AddReceiver(kTestUniverse + i));

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_socket_t* socket,
                                                    socket_close_behavior_t close_behavior) {
    uint16_t universe =
        kTestUniverse + ((static_cast<uint16_t>(sacn_remove_receiver_socket_fake.call_count) - 1u) / 2u);

    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(universe, &state);

    EXPECT_TRUE((socket == &state->ipv4_socket) || (socket == &state->ipv6_socket));
    EXPECT_EQ(close_behavior, kCloseSocketNow);
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_all_receiver_sockets(kCloseSocketNow);

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u * SACN_RECEIVER_MAX_UNIVERSES);
}

TEST_F(TestReceiverThread, AddsPendingSockets)
{
  sacn_add_pending_sockets_fake.custom_fake = [](SacnRecvThreadContext* recv_thread_context) {
    EXPECT_EQ(recv_thread_context, get_recv_thread_context(0u));
  };

  for (unsigned int i = 1u; i <= 10u; ++i)
  {
    RunThreadCycle();
    sacn_add_pending_sockets_fake.call_count = i;
  }
}

TEST_F(TestReceiverThread, CleansDeadSockets)
{
  sacn_cleanup_dead_sockets_fake.custom_fake = [](SacnRecvThreadContext* recv_thread_context) {
    EXPECT_EQ(recv_thread_context, get_recv_thread_context(0u));
  };

  for (unsigned int i = 1u; i <= 10u; ++i)
  {
    RunThreadCycle();
    sacn_cleanup_dead_sockets_fake.call_count = i;
  }
}

TEST_F(TestReceiverThread, Reads)
{
  sacn_read_fake.custom_fake = [](SacnRecvThreadContext* recv_thread_context, SacnReadResult* read_result) {
    EXPECT_EQ(recv_thread_context, get_recv_thread_context(0u));
    EXPECT_NE(read_result, nullptr);
    return kEtcPalErrTimedOut;
  };

  for (unsigned int i = 1u; i <= 10u; ++i)
  {
    RunThreadCycle();
    sacn_read_fake.call_count = i;
  }
}

TEST_F(TestReceiverThread, UniverseDataWorks)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t handle, const EtcPalSockAddr* source_addr,
                                      const SacnHeaderData* header, const uint8_t* pdata, bool is_sampling,
                                      void* context) {
    EXPECT_EQ(handle, kFirstReceiverHandle);
    EXPECT_EQ(etcpal_ip_cmp(&source_addr->ip, &kTestSockAddr.ip), 0);
    EXPECT_EQ(source_addr->port, kTestSockAddr.port);
    EXPECT_EQ(ETCPAL_UUID_CMP(&header->cid, &kTestCid), 0);
    EXPECT_EQ(strcmp(header->source_name, kTestName), 0);
    EXPECT_EQ(header->universe_id, kTestUniverse);
    EXPECT_EQ(header->priority, kTestPriority);
    EXPECT_EQ(header->preview, kTestPreview);
    EXPECT_EQ(header->start_code, 0x00u);
    EXPECT_EQ(header->slot_count, kTestBuffer.size());
    EXPECT_EQ(memcmp(pdata, kTestBuffer.data(), kTestBuffer.size()), 0);
    EXPECT_EQ(is_sampling, true);
    EXPECT_EQ(context, nullptr);
  };

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());

  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, UniverseDataFiltersTerminating)
{
  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_TERMINATED);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);
}

TEST_F(TestReceiverThread, UniverseDataFiltersPreview)
{
  SacnReceiverConfig filter_preview_config = kTestReceiverConfig;
  filter_preview_config.flags |= SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA;
  UpdateTestReceiverConfig(filter_preview_config);

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_PREVIEW);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SacnReceiverConfig allow_preview_config = kTestReceiverConfig;
  UpdateTestReceiverConfig(allow_preview_config);

  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, UniverseDataIndicatesPreview)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->preview, true); };

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_PREVIEW);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->preview, false); };

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestReceiverThread, UniverseDataIndicatesSampling)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*,
                                      bool is_sampling, void*) { EXPECT_EQ(is_sampling, true); };

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*,
                                      bool is_sampling, void*) { EXPECT_EQ(is_sampling, false); };
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 3u);
}

TEST_F(TestReceiverThread, UniverseDataIndicatesCustomStartCodes)
{
  // The first packet must be start code 0x00
  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool,
                                      void*) { EXPECT_EQ(header->start_code, static_cast<uint8_t>(test_iteration)); };
  for (test_iteration = 1; test_iteration < 10; ++test_iteration)
  {
    InitTestData(static_cast<uint8_t>(test_iteration), kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, test_iteration + 1u);
  }
}

TEST_F(TestReceiverThread, UniverseDataWaitsForNullStartCode)
{
  for (uint8_t start_code = 0x01u; start_code < 0x10u; ++start_code)
  {
    InitTestData(start_code, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  InitTestData(0xDDu, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, UniverseDataFiltersUnknownUniverses)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->universe_id, kTestUniverse); };

  for (uint16_t unknown_universe = (kTestUniverse - 10u); unknown_universe < kTestUniverse; ++unknown_universe)
  {
    InitTestData(0x00u, unknown_universe, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  for (uint16_t unknown_universe = (kTestUniverse + 1u); unknown_universe < (kTestUniverse + 10u); ++unknown_universe)
  {
    InitTestData(0x00u, unknown_universe, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 1u);
  }
}

TEST_F(TestReceiverThread, UniverseDataIndicatesPap)
{
  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->start_code, 0xDDu); };

  InitTestData(0xDDu, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestReceiverThread, UniverseDataWaitsForPapAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  for (int wait = 200; wait <= SACN_WAIT_FOR_PRIORITY; wait += 200)
  {
    etcpal_getms_fake.return_val += 200u;
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->start_code, 0xDDu); };

  InitTestData(0xDDu, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->start_code, 0x00u); };

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestReceiverThread, UniverseDataEventuallyStopsWaitingForPap)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData* header,
                                      const uint8_t*, bool, void*) { EXPECT_EQ(header->start_code, 0x00u); };

  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  InitTestData(0x00u, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  for (int wait = 200; wait <= SACN_WAIT_FOR_PRIORITY; wait += 200)
  {
    etcpal_getms_fake.return_val += 200u;
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  etcpal_getms_fake.return_val += 200u;
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}
