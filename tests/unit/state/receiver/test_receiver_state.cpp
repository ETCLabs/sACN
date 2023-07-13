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

FAKE_VOID_FUNC(universe_data, sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
               const SacnRecvUniverseData*, void*);
FAKE_VOID_FUNC(sources_lost, sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*);
FAKE_VOID_FUNC(sampling_period_started, sacn_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(sampling_period_ended, sacn_receiver_t, uint16_t, void*);
FAKE_VOID_FUNC(source_pap_lost, sacn_receiver_t, uint16_t, const SacnRemoteSource*, void*);
FAKE_VOID_FUNC(source_limit_exceeded, sacn_receiver_t, uint16_t, void*);

static constexpr uint16_t kTestUniverse = 123u;
static int kTestContext = 1234567;

#if SACN_DYNAMIC_MEM
static const size_t kNumTestUniverses = 30u;
#else
static const size_t kNumTestUniverses = SACN_RECEIVER_MAX_UNIVERSES;
#endif

static constexpr SacnReceiverCallbacks kTestCallbacks = {
    universe_data,         sources_lost, sampling_period_started, sampling_period_ended, source_pap_lost,
    source_limit_exceeded, &kTestContext};
static constexpr SacnReceiverConfig kTestReceiverConfig = {
    kTestUniverse, kTestCallbacks, {1, DMX_ADDRESS_COUNT}, SACN_RECEIVER_INFINITE_SOURCES, 0u, kSacnIpV4AndIpV6};
static constexpr SacnSourceDetectorConfig kTestSourceDetectorConfig = {
    {[](sacn_remote_source_t, const EtcPalUuid*, const char*, const uint16_t*, size_t, void*) {},
     [](sacn_remote_source_t, const EtcPalUuid*, const char*, void*) {}, nullptr, nullptr},
    SACN_SOURCE_DETECTOR_INFINITE,
    SACN_SOURCE_DETECTOR_INFINITE,
    kSacnIpV4AndIpV6};

static const EtcPalUuid kTestCid = etcpal::Uuid::FromString("5103d586-44bf-46df-8c5a-e690f3dd6e22").get();
static constexpr char kTestName[] = "Test Name";
static const EtcPalSockAddr kTestSockAddr = {SACN_PORT, etcpal::IpAddr::FromString("10.101.1.1").get()};

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};

// Keep the number of v4 interfaces equal to the number of v6 interfaces
static std::vector<SacnMcastInterface> kTestNetints = {
    {{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk}, {{kEtcPalIpTypeV4, 2u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 4u}, kEtcPalErrOk},
    {{kEtcPalIpTypeV6, 5u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 6u}, kEtcPalErrOk}};

static constexpr sacn_receiver_t kFirstReceiverHandle = 0;
static constexpr uint8_t kTestPriority = 77u;
static constexpr bool kTestPreview = false;

static constexpr etcpal_socket_t kTestSocket = static_cast<etcpal_socket_t>(7);
static TerminationSet kTestTermSet = {{0u, 0u}, {nullptr, nullptr, 0u, nullptr, nullptr, nullptr}, nullptr};

static const EtcPalUuid& GetCid(const SacnRemoteSourceInternal& src)
{
  return *(get_remote_source_cid(src.handle));
}

static const EtcPalUuid& GetCid(const SacnLostSourceInternal& src)
{
  return *(get_remote_source_cid(src.handle));
}

static sacn_remote_source_t GetHandle(const EtcPalUuid& cid)
{
  return get_remote_source_handle(&cid);
}

class TestReceiverState : public ::testing::Test
{
protected:
  enum class ReceiverAddMode
  {
    kComplete,
    kWithoutAssigningToThread
  };

  static bool SocketPointsToState(const etcpal_socket_t* socket, const SacnInternalSocketState& state)
  {
#if SACN_RECEIVER_SOCKET_PER_NIC
    // Allow this to point to the end (thus use of <=) since the count might not have incremented yet.
    return ((socket >= state.ipv4_sockets) && (socket <= (state.ipv4_sockets + state.num_ipv4_sockets))) ||
           ((socket >= state.ipv6_sockets) && (socket <= (state.ipv6_sockets + state.num_ipv6_sockets)));
#else
    return (socket == &state.ipv4_socket) || (socket == &state.ipv6_socket);
#endif
  }

  static void WriteNextSocket(etcpal_socket_t* socket)
  {
    *socket = next_socket_;
    ++next_socket_;
  }

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_sockets_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();

    sacn_initialize_receiver_netints_fake.custom_fake = [](SacnInternalNetintArray* receiver_netints, bool,
                                                           EtcPalRbTree* sampling_period_netints,
                                                           const SacnNetintConfig* app_netint_config) {
      EXPECT_NE(app_netint_config, nullptr);

      if (app_netint_config)
      {
#if SACN_DYNAMIC_MEM
        receiver_netints->netints =
            (EtcPalMcastNetintId*)calloc(app_netint_config->num_netints, sizeof(EtcPalMcastNetintId));
#endif
        receiver_netints->num_netints = app_netint_config->num_netints;

        for (size_t i = 0; i < app_netint_config->num_netints; ++i)
        {
          receiver_netints->netints[i] = app_netint_config->netints[i].iface;
          app_netint_config->netints[i].status = kEtcPalErrOk;

          EXPECT_EQ(add_sacn_sampling_period_netint(sampling_period_netints, &receiver_netints->netints[i], false),
                    kEtcPalErrOk);
        }
      }

      return kEtcPalErrOk;
    };

    next_socket_ = kTestSocket;
    sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                   const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
      WriteNextSocket(socket);
      return kEtcPalErrOk;
    };

    ASSERT_EQ(sacn_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_state_init(), kEtcPalErrOk);

    auto& context = *get_recv_thread_context(0);
    for (int i = 0; i < kTestNetints.size(); ++i)
    {
      context.socket_refs[i].socket.handle = kTestSocket + i;
      context.socket_refs[i].socket.ip_type = kTestNetints[i].iface.ip_type;
#if SACN_RECEIVER_SOCKET_PER_NIC
      context.socket_refs[i].socket.ifindex = kTestNetints[i].iface.index;
#endif
    }

    context.num_socket_refs = kTestNetints.size();
  }

  void TearDown() override
  {
    sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_socket_t*, uint16_t,
                                                      const EtcPalMcastNetintId*, size_t, socket_cleanup_behavior_t) {};

    sacn_receiver_state_deinit();
    sacn_receiver_mem_deinit();
  }

  SacnReceiver* AddReceiver(uint16_t universe_id = kTestUniverse,
                            const SacnReceiverCallbacks& callbacks = kTestCallbacks,
                            sacn_ip_support_t ip_supported = kSacnIpV4AndIpV6,
                            ReceiverAddMode add_mode = ReceiverAddMode::kComplete)
  {
    SacnReceiverConfig config = kTestReceiverConfig;
    config.universe_id = universe_id;
    config.callbacks = callbacks;
    config.ip_supported = ip_supported;

    return AddReceiver(config, add_mode);
  }

  SacnReceiver* AddReceiver(const SacnReceiverConfig& config, ReceiverAddMode add_mode = ReceiverAddMode::kComplete)
  {
    SacnReceiver* receiver = nullptr;

    SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
    netint_config.netints = kTestNetints.data();
    netint_config.num_netints = kTestNetints.size();

    next_socket_ = kTestSocket;
    EXPECT_EQ(add_sacn_receiver(next_receiver_handle_++, &config, &netint_config, NULL, &receiver), kEtcPalErrOk);
    if (receiver)
    {
      EXPECT_EQ(initialize_receiver_sockets(&receiver->sockets), kEtcPalErrOk);
      if (add_mode == ReceiverAddMode::kComplete)
      {
        EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrOk);
      }
    }

    return receiver;
  }

  SacnSourceDetector* AddSourceDetector(const SacnSourceDetectorConfig& config = kTestSourceDetectorConfig)
  {
    SacnSourceDetector* detector = nullptr;

    SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
    netint_config.netints = kTestNetints.data();
    netint_config.num_netints = kTestNetints.size();

    EXPECT_EQ(add_sacn_source_detector(&config, &netint_config, &detector), kEtcPalErrOk);

    return detector;
  }

  SacnTrackedSource* AddTrackedSource(SacnReceiver* receiver)
  {
    SacnTrackedSource* source = nullptr;
    EXPECT_EQ(add_sacn_tracked_source(receiver, &next_source_cid_, kTestName, &kTestNetints[0].iface, 0u, 0u, &source),
              kEtcPalErrOk);

    ++next_source_cid_.data[0];

    return source;
  }

  static etcpal_socket_t next_socket_;

  sacn_receiver_t next_receiver_handle_ = kFirstReceiverHandle;
  EtcPalUuid next_source_cid_ = kTestCid;
};

etcpal_socket_t TestReceiverState::next_socket_;

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

    seq_num_ = 0u;

    memset(test_data_, 0, SACN_MTU);
  }

  void TearDown() override
  {
    if (test_receiver_)
    {
      remove_receiver_from_thread(test_receiver_);
      remove_sacn_receiver(test_receiver_);
      test_receiver_ = nullptr;
    }

    TestReceiverState::TearDown();
  }

  void InitTestData(uint8_t start_code, uint16_t universe, const uint8_t* data = nullptr, size_t data_len = 0u,
                    uint8_t flags = 0u, const EtcPalUuid& source_cid = kTestCid, uint8_t sequence_number = seq_num_,
                    const EtcPalMcastNetintId& netint = kTestNetints[0].iface)
  {
    init_sacn_data_send_buf(test_data_, start_code, &source_cid, kTestName, kTestPriority, universe, 0u, kTestPreview);

    if (data)
      update_send_buf_data(test_data_, data, static_cast<uint16_t>(data_len), kDisableForceSync);

    test_data_[SACN_OPTS_OFFSET] |= flags;
    test_data_[SACN_SEQ_OFFSET] = sequence_number;
    test_data_netint_ = netint;

    sacn_read_fake.custom_fake = [](SacnRecvThreadContext*, SacnReadResult* read_result) {
      read_result->from_addr = kTestSockAddr;
      read_result->data = test_data_;
      read_result->data_len = SACN_MTU;
      read_result->netint = test_data_netint_;
      return kEtcPalErrOk;
    };
  }

  void RemoveTestData()
  {
    memset(test_data_, 0, SACN_MTU);
    sacn_read_fake.custom_fake = [](SacnRecvThreadContext*, SacnReadResult*) { return kEtcPalErrTimedOut; };
  }

  void UpdateTestReceiverConfig(const SacnReceiverConfig& config)
  {
    remove_receiver_from_thread(test_receiver_);
    remove_sacn_receiver(test_receiver_);

    test_receiver_ = AddReceiver(config);
    begin_sampling_period(test_receiver_);
  }

  void RunThreadCycle()
  {
    read_network_and_process(get_recv_thread_context(0u));
    ++seq_num_;
    test_data_[SACN_SEQ_OFFSET] = seq_num_;
  }

  void TestSourceLimitExceeded(int configured_max, int expected_max)
  {
    SacnReceiverConfig config_with_limit = kTestReceiverConfig;
    config_with_limit.source_count_max = configured_max;
    UpdateTestReceiverConfig(config_with_limit);

    source_limit_exceeded_fake.custom_fake = [](sacn_receiver_t, uint16_t universe, void* context) {
      EXPECT_EQ(universe, kTestUniverse);
      EXPECT_EQ(context, &kTestContext);
    };

    static EtcPalUuid last_cid;  // Save this to remove a source later.
    for (int i = 0; i < expected_max; ++i)
    {
      last_cid = etcpal::Uuid::V4().get();
      InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, last_cid);
      RunThreadCycle();
    }

    EXPECT_EQ(source_limit_exceeded_fake.call_count, 0u);

    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u,
                 etcpal::Uuid::V4().get());
    RunThreadCycle();

    EXPECT_EQ(source_limit_exceeded_fake.call_count, 1u);

    // Now test rate limiting.
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u,
                 etcpal::Uuid::V4().get());
    RunThreadCycle();
    EXPECT_EQ(source_limit_exceeded_fake.call_count, 1u);

    // Remove a source.
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_TERMINATED,
                 last_cid);
    etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
    get_expired_sources_fake.custom_fake = [](TerminationSet**, SourcesLostNotification* sources_lost) {
      add_lost_source(sources_lost, GetHandle(last_cid), &last_cid, kTestName, true);
    };
    RunThreadCycle();
    EXPECT_EQ(source_limit_exceeded_fake.call_count, 1u);

    // Now add two sources - one to get back to the limit, and another to surpass it.
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u,
                 etcpal::Uuid::V4().get());
    RunThreadCycle();
    EXPECT_EQ(source_limit_exceeded_fake.call_count, 1u);
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u,
                 etcpal::Uuid::V4().get());
    RunThreadCycle();
    EXPECT_EQ(source_limit_exceeded_fake.call_count, 2u);
  }

  SacnReceiver* test_receiver_;
  static uint8_t seq_num_;
  static uint8_t test_data_[SACN_MTU];
  static EtcPalMcastNetintId test_data_netint_;
};

uint8_t TestReceiverThread::seq_num_ = 0u;
uint8_t TestReceiverThread::test_data_[SACN_MTU] = {0};
EtcPalMcastNetintId TestReceiverThread::test_data_netint_ = kTestNetints[0].iface;

TEST_F(TestReceiverState, RespectsMaxReceiverLimit)
{
  for (int i = 0; i < kNumTestUniverses; ++i)
    AddReceiver(static_cast<uint16_t>(i + 1));
}

TEST_F(TestReceiverState, RespectsMaxTrackedSourceLimit)
{
  for (int i = 0; i < kNumTestUniverses; ++i)
  {
    SacnReceiver* receiver = AddReceiver(static_cast<uint16_t>(i + 1));

    SacnTrackedSource* source = nullptr;
    for (int j = 0; (j < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE) &&
                    (((i * SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE) + j) < SACN_RECEIVER_TOTAL_MAX_SOURCES);
         ++j)
    {
      EtcPalUuid cid = etcpal::Uuid::V4().get();
      EXPECT_EQ(add_sacn_tracked_source(receiver, &cid, "name", &kTestNetints[0].iface, 0u, 0u, &source), kEtcPalErrOk);
    }
  }
}

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

  AddReceiver(kTestUniverse);

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
  AddReceiver(kTestUniverse);

  sacn_remove_receiver_socket_fake.custom_fake =
      [](sacn_thread_id_t, etcpal_socket_t*, uint16_t, const EtcPalMcastNetintId*, size_t,
         socket_cleanup_behavior_t cleanup_behavior) { EXPECT_EQ(cleanup_behavior, kPerformAllSocketCleanupNow); };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_receiver_state_deinit();

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size());
#else
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
#endif
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
    WriteNextSocket(socket);
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

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);

  AddReceiver();

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, kTestNetints.size());
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
#endif
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 1u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(get_recv_thread_context(0)->running, true);
  EXPECT_EQ(get_recv_thread_context(0)->receivers->keys.universe, kTestUniverse);
}

TEST_F(TestReceiverState, AssignReceiverToThreadHandlesSocketError)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t,
                                                 etcpal_socket_t*) { return kEtcPalErrNoNetints; };

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);

  auto* receiver =
      AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4AndIpV6, ReceiverAddMode::kWithoutAssigningToThread);
  ASSERT_TRUE(receiver);

  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrNoNetints);

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(get_recv_thread_context(0)->running, false);
  EXPECT_EQ(get_recv_thread_context(0)->receivers, nullptr);
}

TEST_F(TestReceiverState, AssignReceiverToThreadHandlesThreadError)
{
  etcpal_thread_create_fake.return_val = kEtcPalErrSys;

  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 0u);
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 0u);

  auto* receiver =
      AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4AndIpV6, ReceiverAddMode::kWithoutAssigningToThread);
  ASSERT_TRUE(receiver);

  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrSys);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, kTestNetints.size());
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size());
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
#endif
  EXPECT_EQ(etcpal_thread_create_fake.call_count, 1u);
  EXPECT_EQ(get_recv_thread_context(0)->running, false);
  EXPECT_EQ(get_recv_thread_context(0)->receivers, nullptr);
}

TEST_F(TestReceiverState, RemoveReceiverFromThreadWorks)
{
  SacnReceiver* receiver = AddReceiver();

  remove_receiver_from_thread(receiver);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size());
#else
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
#endif
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

    for (size_t i = 0u; i < num_netints; ++i)
    {
      bool found = false;
      for (size_t j = 0u; j < kTestNetints.size(); ++j)
      {
        if ((netints[i].index == kTestNetints[j].iface.index) && (netints[i].ip_type == kTestNetints[j].iface.ip_type))
        {
          found = true;
          break;
        }
      }

      EXPECT_TRUE(found);
    }

#if SACN_RECEIVER_SOCKET_PER_NIC
    EXPECT_EQ(num_netints, 1u);
#else
    EXPECT_EQ(num_netints, kTestNetints.size());
#endif

    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));

    WriteNextSocket(socket);

    return kEtcPalErrOk;
  };

  AddReceiver();  // Calls add_receiver_sockets

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, kTestNetints.size());
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
#endif
}

TEST_F(TestReceiverState, AddReceiverSocketsHandlesIpv4Only)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(ip_type, kEtcPalIpTypeV4);
    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));

    WriteNextSocket(socket);

    return kEtcPalErrOk;
  };

  AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4Only);
#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, kTestNetints.size() / 2);
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 1u);
#endif
}

TEST_F(TestReceiverState, AddReceiverSocketsHandlesIpv6Only)
{
  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(ip_type, kEtcPalIpTypeV6);
    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));

    WriteNextSocket(socket);

    return kEtcPalErrOk;
  };

  AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV6Only);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, kTestNetints.size() / 2);
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 1u);
#endif
}

TEST_F(TestReceiverState, AddReceiverSocketsHandlesErrors)
{
  auto* receiver =
      AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4AndIpV6, ReceiverAddMode::kWithoutAssigningToThread);
  ASSERT_TRUE(receiver);

  EXPECT_EQ(initialize_receiver_sockets(&receiver->sockets), kEtcPalErrOk);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t,
                                                 etcpal_socket_t*) { return kEtcPalErrNoNetints; };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrNoNetints);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);  // 1 call for IPv4, 1 for IPv6
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t*) {
    return (ip_type == kEtcPalIpTypeV4) ? kEtcPalErrNoNetints : kEtcPalErrSys;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrSys);
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 4u);  // 1 call for IPv4, 1 for IPv6
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    if (ip_type == kEtcPalIpTypeV4)
      return kEtcPalErrNoNetints;
    WriteNextSocket(socket);
    return kEtcPalErrOk;
  };

  EXPECT_EQ(add_receiver_sockets(receiver), kEtcPalErrOk);
#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 8u);  // 1 call for IPv4, 3 for IPv6
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 6u);  // 1 call for IPv4, 1 for IPv6
#endif
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                 const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
    if (ip_type == kEtcPalIpTypeV6)
      return kEtcPalErrNoNetints;
    WriteNextSocket(socket);
    return kEtcPalErrOk;
  };

  // Call assign_receiver_to_thread at the end to finish receiver init (so TearDown works correctly)
  EXPECT_EQ(assign_receiver_to_thread(receiver), kEtcPalErrOk);
#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 12u);  // 3 calls for IPv4, 1 for IPv6
#else
  EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 8u);  // 1 call for IPv4, 1 for IPv6
#endif
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);
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
  EXPECT_EQ(etcpal_timer_remaining(&receiver->sample_timer), static_cast<uint32_t>(SACN_SAMPLE_TIME));

  // Also test that the timer doesn't reset when a sampling period is already occurring
  ++etcpal_getms_fake.return_val;
  begin_sampling_period(receiver);

  EXPECT_EQ(receiver->sampling, true);
  EXPECT_EQ(receiver->notified_sampling_started, false);
  EXPECT_EQ(receiver->sample_timer.interval, static_cast<uint32_t>(SACN_SAMPLE_TIME));
  EXPECT_EQ(etcpal_timer_remaining(&receiver->sample_timer), static_cast<uint32_t>(SACN_SAMPLE_TIME) - 1u);
}

TEST_F(TestReceiverState, RemoveReceiverSocketsRemovesIpv4AndIpv6)
{
  SacnReceiver* receiver = AddReceiver();

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket, uint16_t,
                                                    const EtcPalMcastNetintId*, size_t,
                                                    socket_cleanup_behavior_t cleanup_behavior) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, 0u);
    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));
    EXPECT_EQ(cleanup_behavior, kPerformAllSocketCleanupNow);

    *socket = ETCPAL_SOCKET_INVALID;
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_receiver_sockets(receiver, kPerformAllSocketCleanupNow);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size());
#else
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
#endif
}

TEST_F(TestReceiverState, RemoveReceiverSocketsRemovesOnlyIpv4)
{
  SacnReceiver* receiver = AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV4Only);

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket, uint16_t,
                                                    const EtcPalMcastNetintId*, size_t,
                                                    socket_cleanup_behavior_t cleanup_behavior) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, 0u);
    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));
    EXPECT_EQ(cleanup_behavior, kPerformAllSocketCleanupNow);

    *socket = ETCPAL_SOCKET_INVALID;
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_receiver_sockets(receiver, kPerformAllSocketCleanupNow);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size() / 2);
#else
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);
#endif
}

TEST_F(TestReceiverState, RemoveReceiverSocketsRemovesOnlyIpv6)
{
  SacnReceiver* receiver = AddReceiver(kTestUniverse, kTestCallbacks, kSacnIpV6Only);

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket, uint16_t,
                                                    const EtcPalMcastNetintId*, size_t,
                                                    socket_cleanup_behavior_t cleanup_behavior) {
    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(kTestUniverse, &state);

    EXPECT_EQ(thread_id, 0u);
    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));
    EXPECT_EQ(cleanup_behavior, kPerformAllSocketCleanupNow);

    *socket = ETCPAL_SOCKET_INVALID;
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_receiver_sockets(receiver, kPerformAllSocketCleanupNow);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size() / 2);
#else
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);
#endif
}

TEST_F(TestReceiverState, RemoveAllReceiverSocketsWorks)
{
#if SACN_RECEIVER_SOCKET_PER_NIC
  static const size_t kSocketsPerUniverse = kTestNetints.size();
#else
  static const size_t kSocketsPerUniverse = 2u;
#endif

  for (uint16_t i = 0u; i < kNumTestUniverses; ++i)
    AddReceiver(kTestUniverse + i);

  sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_socket_t* socket, uint16_t,
                                                    const EtcPalMcastNetintId*, size_t,
                                                    socket_cleanup_behavior_t cleanup_behavior) {
    uint16_t universe = static_cast<uint16_t>(
        kTestUniverse + ((sacn_remove_receiver_socket_fake.call_count - 1u) / kSocketsPerUniverse));

    SacnReceiver* state = nullptr;
    lookup_receiver_by_universe(universe, &state);

    ASSERT_TRUE(state);
    EXPECT_TRUE(SocketPointsToState(socket, state->sockets));
    EXPECT_EQ(cleanup_behavior, kPerformAllSocketCleanupNow);
  };

  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 0u);

  remove_all_receiver_sockets(kPerformAllSocketCleanupNow);

#if SACN_RECEIVER_SOCKET_PER_NIC
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, kTestNetints.size() * kNumTestUniverses);
#else
  EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u * kNumTestUniverses);
#endif
}

TEST_F(TestReceiverState, TerminateSourcesOnRemovedNetintsWorks)
{
  SacnReceiver* receiver = AddReceiver();
  ASSERT_EQ(receiver->netints.num_netints, kTestNetints.size());

  std::vector<SacnTrackedSource*> sources;
  for (const auto& netint : kTestNetints)
  {
    EtcPalUuid cid = etcpal::Uuid::V4().get();
    SacnTrackedSource* source = nullptr;
    EXPECT_EQ(add_sacn_tracked_source(receiver, &cid, "name", &netint.iface, 0u, 0u, &source), kEtcPalErrOk);
    sources.push_back(source);
  }

  while (receiver->netints.num_netints > 0)
  {
    terminate_sources_on_removed_netints(receiver);

    for (size_t i = 0; i < receiver->netints.num_netints; ++i)
    {
      EXPECT_FALSE(sources[i]->terminated);
    }

    for (size_t i = receiver->netints.num_netints; i < sources.size(); ++i)
    {
      EXPECT_TRUE(sources[i]->terminated);
    }

    --receiver->netints.num_netints;
  }
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
  universe_data_fake.custom_fake = [](sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                      const SacnRemoteSource* source_info, const SacnRecvUniverseData* universe_data,
                                      void* context) {
    EXPECT_EQ(receiver_handle, kFirstReceiverHandle);
    EXPECT_EQ(etcpal_ip_cmp(&source_addr->ip, &kTestSockAddr.ip), 0);
    EXPECT_EQ(source_addr->port, kTestSockAddr.port);
    EXPECT_EQ(ETCPAL_UUID_CMP(&source_info->cid, &kTestCid), 0);
    EXPECT_EQ(strcmp(source_info->name, kTestName), 0);
    EXPECT_EQ(universe_data->universe_id, kTestUniverse);
    EXPECT_EQ(universe_data->priority, kTestPriority);
    EXPECT_EQ(universe_data->preview, kTestPreview);
    EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_DMX);
    EXPECT_EQ(universe_data->slot_range.address_count, kTestBuffer.size());
    EXPECT_EQ(memcmp(universe_data->values, kTestBuffer.data(), kTestBuffer.size()), 0);
    EXPECT_EQ(universe_data->is_sampling, true);
    EXPECT_EQ(context, &kTestContext);
  };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());

  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, UniverseDataSourceHandleWorks)
{
  static sacn_remote_source_t first_handle = SACN_REMOTE_SOURCE_INVALID;

  EtcPalUuid cid1 = etcpal::Uuid::V4().get();
  EtcPalUuid cid2 = etcpal::Uuid::V4().get();
  EtcPalUuid cid3 = etcpal::Uuid::V4().get();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource* source_info,
                                      const SacnRecvUniverseData*, void*) {
    first_handle = source_info->handle;
    EXPECT_NE(first_handle, SACN_REMOTE_SOURCE_INVALID);
  };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cid1);
  RunThreadCycle();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource* source_info,
                                      const SacnRecvUniverseData*,
                                      void*) { EXPECT_EQ(source_info->handle, first_handle + 1u); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cid2);
  RunThreadCycle();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource* source_info,
                                      const SacnRecvUniverseData*,
                                      void*) { EXPECT_EQ(source_info->handle, first_handle + 2u); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cid3);
  RunThreadCycle();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource* source_info,
                                      const SacnRecvUniverseData*,
                                      void*) { EXPECT_EQ(source_info->handle, first_handle + 2u); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cid3);
  RunThreadCycle();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource* source_info,
                                      const SacnRecvUniverseData*,
                                      void*) { EXPECT_EQ(source_info->handle, first_handle + 1u); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cid2);
  RunThreadCycle();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource* source_info,
                                      const SacnRecvUniverseData*,
                                      void*) { EXPECT_EQ(source_info->handle, first_handle); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cid1);
  RunThreadCycle();

  EXPECT_EQ(universe_data_fake.call_count, 6u);
}

TEST_F(TestReceiverThread, UniverseDataFiltersTerminating)
{
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_TERMINATED);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);
}

TEST_F(TestReceiverThread, UniverseDataFiltersPreview)
{
  SacnReceiverConfig filter_preview_config = kTestReceiverConfig;
  filter_preview_config.flags |= SACN_RECEIVER_OPTS_FILTER_PREVIEW_DATA;
  UpdateTestReceiverConfig(filter_preview_config);

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_PREVIEW);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  SacnReceiverConfig allow_preview_config = kTestReceiverConfig;
  UpdateTestReceiverConfig(allow_preview_config);

  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, UniverseDataFiltersFutureSamplingPeriodNetints)
{
  std::vector<SacnMcastInterface> current_netints = {kTestNetints.begin(),
                                                     kTestNetints.begin() + (kTestNetints.size() / 2)};
  std::vector<SacnMcastInterface> future_netints = {kTestNetints.begin() + (kTestNetints.size() / 2),
                                                    kTestNetints.end()};

  etcpal_rbtree_clear_with_cb(&test_receiver_->sampling_period_netints, sampling_period_netint_tree_dealloc);
  for (const auto& netint : current_netints)
    add_sacn_sampling_period_netint(&test_receiver_->sampling_period_netints, &netint.iface, false);
  for (const auto& netint : future_netints)
    add_sacn_sampling_period_netint(&test_receiver_->sampling_period_netints, &netint.iface, true);

  size_t prev_call_count = universe_data_fake.call_count;
  for (const auto& netint : current_netints)
  {
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u,
                 etcpal::Uuid::V4().get(), seq_num_, netint.iface);
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, prev_call_count + 1);
    prev_call_count = universe_data_fake.call_count;
  }

  for (const auto& netint : future_netints)
  {
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u,
                 etcpal::Uuid::V4().get(), seq_num_, netint.iface);
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, prev_call_count);
  }
}

TEST_F(TestReceiverThread, UniverseDataFiltersSubsequentSourceNetints)
{
  static constexpr size_t kNumTestIterations = 3;
  static constexpr size_t kNumSources = 3;
  static constexpr size_t kNumDataCallsPerSource = 3;

  EXPECT_EQ(universe_data_fake.call_count, 0u);

  std::vector<EtcPalUuid> cids;
  for (size_t i = 0; i < kNumSources; ++i)
    cids.push_back(etcpal::Uuid::V4().get());

  // Elapse sampling period - filtering only takes place after
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  for (size_t i = 0; i < kNumTestIterations; ++i)
  {
    for (size_t j = 0; j < kNumSources; ++j)
    {
      // The netints come in a different order for each source - only the first one for a source should notify.
      std::vector<SacnMcastInterface> ordered_netints;
      for (size_t k = 0; k < kTestNetints.size(); ++k)
        ordered_netints.push_back(kTestNetints[(j + k) % kTestNetints.size()]);

      for (size_t k = 0; k < kNumDataCallsPerSource; ++k)
      {
        for (const auto& netint : ordered_netints)
        {
          // Select whichever start code will be allowed immediately through to the callback
#if SACN_ETC_PRIORITY_EXTENSION
          uint8_t start_code = SACN_STARTCODE_PRIORITY;
#else
          uint8_t start_code = SACN_STARTCODE_DMX;
#endif
          InitTestData(start_code, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, cids[j], seq_num_,
                       netint.iface);
          RunThreadCycle();
          EXPECT_EQ(universe_data_fake.call_count,
                    (i * kNumSources * kNumDataCallsPerSource) + (j * kNumDataCallsPerSource) + k + 1)
              << "(i:" << i << " j:" << j << " k:" << k << ")";
        }
      }
    }
  }
}

TEST_F(TestReceiverThread, UniverseDataIndicatesPreview)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->preview, true); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_PREVIEW);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->preview, false); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestReceiverThread, UniverseDataIndicatesSampling)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->is_sampling, true); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->is_sampling, false); };
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 3u);
}

TEST_F(TestReceiverThread, CustomStartCodesNotifyCorrectlyDuringSamplingPeriod)
{
  static unsigned int test_iteration;

  for (uint8_t i = 1u; i < 10u; ++i)
  {
    InitTestData(i, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data, void*) {
    EXPECT_EQ(universe_data->start_code, static_cast<uint8_t>(test_iteration));
  };
  for (test_iteration = 1u; test_iteration < 10u; ++test_iteration)
  {
    InitTestData(static_cast<uint8_t>(test_iteration), kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, test_iteration + 1u);
  }
}

TEST_F(TestReceiverThread, UniverseDataFiltersUnknownUniverses)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->universe_id, kTestUniverse); };

  for (uint16_t unknown_universe = (kTestUniverse - 10u); unknown_universe < kTestUniverse; ++unknown_universe)
  {
    InitTestData(SACN_STARTCODE_DMX, unknown_universe, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  for (uint16_t unknown_universe = (kTestUniverse + 1u); unknown_universe < (kTestUniverse + 10u); ++unknown_universe)
  {
    InitTestData(SACN_STARTCODE_DMX, unknown_universe, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 1u);
  }
}

TEST_F(TestReceiverThread, PapNotifiesCorrectlyDuringSamplingPeriod)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_PRIORITY); };

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
#if SACN_ETC_PRIORITY_EXTENSION
  EXPECT_EQ(universe_data_fake.call_count, 1u);
#else
  EXPECT_EQ(universe_data_fake.call_count, 0u);
#endif

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_DMX); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
#if SACN_ETC_PRIORITY_EXTENSION
  EXPECT_EQ(universe_data_fake.call_count, 2u);
#else
  EXPECT_EQ(universe_data_fake.call_count, 1u);
#endif

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_PRIORITY); };

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

#if SACN_ETC_PRIORITY_EXTENSION
  EXPECT_EQ(universe_data_fake.call_count, 3u);
#else
  EXPECT_EQ(universe_data_fake.call_count, 1u);
#endif
}

TEST_F(TestReceiverThread, UniverseDataHandlesPurePapAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

#if SACN_ETC_PRIORITY_EXTENSION
  unsigned int expected_data_call_count = 1u;
#else
  unsigned int expected_data_call_count = 0u;
#endif
  EXPECT_EQ(universe_data_fake.call_count, expected_data_call_count);
  for (int wait = 200; wait <= SACN_WAIT_FOR_PRIORITY; wait += 200)
  {
    etcpal_getms_fake.return_val += 200u;
    RunThreadCycle();

#if SACN_ETC_PRIORITY_EXTENSION
    ++expected_data_call_count;
#endif
    EXPECT_EQ(universe_data_fake.call_count, expected_data_call_count);
  }

  etcpal_getms_fake.return_val += 200u;
  RunThreadCycle();

#if SACN_ETC_PRIORITY_EXTENSION
  ++expected_data_call_count;
#endif
  EXPECT_EQ(universe_data_fake.call_count, expected_data_call_count);
}

TEST_F(TestReceiverThread, CustomStartCodesNotifyCorrectlyAfterSamplingPeriod)
{
  static unsigned int test_iteration;

  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  for (uint8_t i = 1u; i < 10u; ++i)
  {
    InitTestData(i, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

  unsigned int start_count = universe_data_fake.call_count;

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data, void*) {
    EXPECT_EQ(universe_data->start_code, static_cast<uint8_t>(test_iteration));
  };
  for (test_iteration = 1u; test_iteration < 10u; ++test_iteration)
  {
    InitTestData(static_cast<uint8_t>(test_iteration), kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, start_count + test_iteration);
  }
}

TEST_F(TestReceiverThread, SourceLossProcessedEachTick)
{
  for (unsigned int ticks = 0u; ticks < 10u; ++ticks)
  {
    RunThreadCycle();
    EXPECT_EQ(mark_sources_online_fake.call_count, ticks);
    EXPECT_EQ(mark_sources_offline_fake.call_count, ticks);
    EXPECT_EQ(get_expired_sources_fake.call_count, ticks);
    etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  }
}

TEST_F(TestReceiverThread, SourceLossUsesExpiredWait)
{
  static constexpr uint32_t kTestExpiredWait = 1234u;

  mark_sources_offline_fake.custom_fake = [](uint16_t, const SacnLostSourceInternal*, size_t,
                                             const SacnRemoteSourceInternal*, size_t, TerminationSet**,
                                             uint32_t expired_wait) {
    EXPECT_EQ(expired_wait, kTestExpiredWait);
    return kEtcPalErrOk;
  };

  EXPECT_EQ(mark_sources_offline_fake.call_count, 0u);

  set_expired_wait(kTestExpiredWait);
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();

  EXPECT_EQ(mark_sources_offline_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, SourceGoesOnlineCorrectly)
{
  auto source_is_online = [](uint16_t, const SacnRemoteSourceInternal* online_sources, size_t num_online_sources,
                             TerminationSet**) {
    EXPECT_EQ(num_online_sources, 1u);
    EXPECT_EQ(ETCPAL_UUID_CMP(&GetCid(online_sources[0]), &kTestCid), 0);
  };
  auto source_is_not_online = [](uint16_t, const SacnRemoteSourceInternal*, size_t num_online_sources,
                                 TerminationSet**) { EXPECT_EQ(num_online_sources, 0u); };

  // Online
  mark_sources_online_fake.custom_fake = source_is_online;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_online_fake.call_count, 1u);

  // Unknown
  mark_sources_online_fake.custom_fake = source_is_not_online;

  RemoveTestData();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_online_fake.call_count, 2u);

  // Online
  mark_sources_online_fake.custom_fake = source_is_online;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_online_fake.call_count, 3u);
}

TEST_F(TestReceiverThread, SourceGoesUnknownCorrectly)
{
  auto source_is_unknown = [](uint16_t, const SacnLostSourceInternal*, size_t,
                              const SacnRemoteSourceInternal* unknown_sources, size_t num_unknown_sources,
                              TerminationSet**, uint32_t) {
    EXPECT_EQ(num_unknown_sources, 1u);
    EXPECT_EQ(ETCPAL_UUID_CMP(&GetCid(unknown_sources[0]), &kTestCid), 0);
    return kEtcPalErrOk;
  };
  auto source_is_not_unknown = [](uint16_t, const SacnLostSourceInternal*, size_t, const SacnRemoteSourceInternal*,
                                  size_t num_unknown_sources, TerminationSet**, uint32_t) {
    EXPECT_EQ(num_unknown_sources, 0u);
    return kEtcPalErrOk;
  };

  // Online
  mark_sources_offline_fake.custom_fake = source_is_not_unknown;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 1u);

  // Unknown
  mark_sources_offline_fake.custom_fake = source_is_unknown;

  RemoveTestData();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 2u);

  // Online
  mark_sources_offline_fake.custom_fake = source_is_not_unknown;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 3u);

  // Unknown
  mark_sources_offline_fake.custom_fake = source_is_unknown;

  RemoveTestData();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 4u);

  // Offline
  mark_sources_offline_fake.custom_fake = source_is_not_unknown;

  etcpal_getms_fake.return_val += (SACN_SOURCE_LOSS_TIMEOUT + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 5u);
}

TEST_F(TestReceiverThread, TimedOutSourceGoesOfflineCorrectly)
{
  auto source_is_offline = [](uint16_t, const SacnLostSourceInternal* offline_sources, size_t num_offline_sources,
                              const SacnRemoteSourceInternal*, size_t, TerminationSet**, uint32_t) {
    EXPECT_EQ(num_offline_sources, 1u);
    EXPECT_EQ(ETCPAL_UUID_CMP(&GetCid(offline_sources[0]), &kTestCid), 0);
    return kEtcPalErrOk;
  };
  auto source_is_not_offline = [](uint16_t, const SacnLostSourceInternal*, size_t num_offline_sources,
                                  const SacnRemoteSourceInternal*, size_t, TerminationSet**, uint32_t) {
    EXPECT_EQ(num_offline_sources, 0u);
    return kEtcPalErrOk;
  };

  // Online
  mark_sources_offline_fake.custom_fake = source_is_not_offline;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 1u);

  // Unknown
  mark_sources_offline_fake.custom_fake = source_is_not_offline;

  RemoveTestData();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 2u);

  // Offline
  mark_sources_offline_fake.custom_fake = source_is_offline;

  etcpal_getms_fake.return_val += (SACN_SOURCE_LOSS_TIMEOUT + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 3u);
}

TEST_F(TestReceiverThread, TerminatedSourceGoesOfflineCorrectly)
{
  auto source_is_offline = [](uint16_t, const SacnLostSourceInternal* offline_sources, size_t num_offline_sources,
                              const SacnRemoteSourceInternal*, size_t, TerminationSet**, uint32_t) {
    EXPECT_EQ(num_offline_sources, 1u);
    EXPECT_EQ(ETCPAL_UUID_CMP(&GetCid(offline_sources[0]), &kTestCid), 0);
    return kEtcPalErrOk;
  };
  auto source_is_not_offline = [](uint16_t, const SacnLostSourceInternal*, size_t num_offline_sources,
                                  const SacnRemoteSourceInternal*, size_t, TerminationSet**, uint32_t) {
    EXPECT_EQ(num_offline_sources, 0u);
    return kEtcPalErrOk;
  };

  // Online
  mark_sources_offline_fake.custom_fake = source_is_not_offline;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 1u);

  // Offline
  mark_sources_offline_fake.custom_fake = source_is_offline;

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_TERMINATED);
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();
  EXPECT_EQ(mark_sources_offline_fake.call_count, 2u);
}

TEST_F(TestReceiverThread, StatusListsTrackMultipleSources)
{
  static EtcPalUuid online_cid_1 = etcpal::Uuid::V4().get();
  static EtcPalUuid online_cid_2 = etcpal::Uuid::V4().get();
  static EtcPalUuid unknown_cid_1 = etcpal::Uuid::V4().get();
  static EtcPalUuid unknown_cid_2 = etcpal::Uuid::V4().get();
  static EtcPalUuid offline_cid_1 = etcpal::Uuid::V4().get();
  static EtcPalUuid offline_cid_2 = etcpal::Uuid::V4().get();

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, offline_cid_1);
  RunThreadCycle();
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_TERMINATED,
               offline_cid_1);
  RunThreadCycle();
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, offline_cid_2);
  RunThreadCycle();
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), SACN_OPTVAL_TERMINATED,
               offline_cid_2);
  RunThreadCycle();

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, unknown_cid_1);
  RunThreadCycle();
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, unknown_cid_2);
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, online_cid_1);
  RunThreadCycle();
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, online_cid_2);
  RunThreadCycle();

  static auto list_includes_cids = [](const EtcPalUuid& list_cid_1, const EtcPalUuid& list_cid_2,
                                      const EtcPalUuid& actual_cid_1, const EtcPalUuid& actual_cid_2) {
    if ((ETCPAL_UUID_CMP(&list_cid_1, &actual_cid_1) == 0) && (ETCPAL_UUID_CMP(&list_cid_2, &actual_cid_2) == 0))
      return true;
    else if ((ETCPAL_UUID_CMP(&list_cid_1, &actual_cid_2) == 0) && (ETCPAL_UUID_CMP(&list_cid_2, &actual_cid_1) == 0))
      return true;
    else
      return false;
  };

  mark_sources_offline_fake.custom_fake =
      [](uint16_t, const SacnLostSourceInternal* offline_sources, size_t num_offline_sources,
         const SacnRemoteSourceInternal* unknown_sources, size_t num_unknown_sources, TerminationSet**, uint32_t) {
        EXPECT_EQ(num_offline_sources, 2u);
        EXPECT_TRUE(
            list_includes_cids(GetCid(offline_sources[0]), GetCid(offline_sources[1]), offline_cid_1, offline_cid_2));
        EXPECT_EQ(num_unknown_sources, 2u);
        EXPECT_TRUE(
            list_includes_cids(GetCid(unknown_sources[0]), GetCid(unknown_sources[1]), unknown_cid_1, unknown_cid_2));

        return kEtcPalErrOk;
      };

  mark_sources_online_fake.custom_fake = [](uint16_t, const SacnRemoteSourceInternal* online_sources,
                                            size_t num_online_sources, TerminationSet**) {
    EXPECT_EQ(num_online_sources, 2u);
    EXPECT_TRUE(list_includes_cids(GetCid(online_sources[0]), GetCid(online_sources[1]), online_cid_1, online_cid_2));
  };

  unsigned int expected_mark_sources_offline_count = mark_sources_offline_fake.call_count + 1u;
  unsigned int expected_mark_sources_online_count = mark_sources_online_fake.call_count + 1u;

  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();

  EXPECT_EQ(mark_sources_offline_fake.call_count, expected_mark_sources_offline_count);
  EXPECT_EQ(mark_sources_online_fake.call_count, expected_mark_sources_online_count);
}

TEST_F(TestReceiverThread, SourcesLostWorks)
{
  static constexpr size_t kNumLostSources = 7u;
  static EtcPalUuid lost_source_cids[kNumLostSources];

  for (size_t i = 0u; i < kNumLostSources; ++i)
  {
    EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), i);
    lost_source_cids[i] = etcpal::Uuid::V4().get();
    InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, lost_source_cids[i]);
    RunThreadCycle();
    EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), i + 1u);
  }

  get_expired_sources_fake.custom_fake = [](TerminationSet**, SourcesLostNotification* sources_lost) {
    for (size_t i = 0u; i < kNumLostSources; ++i)
      add_lost_source(sources_lost, GetHandle(lost_source_cids[i]), &lost_source_cids[i], kTestName, (i % 2) == 0);
  };

  sources_lost_fake.custom_fake = [](sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources,
                                     size_t num_lost_sources, void* context) {
    EXPECT_EQ(handle, kFirstReceiverHandle);
    EXPECT_EQ(universe, kTestUniverse);

    for (size_t i = 0u; i < kNumLostSources; ++i)
    {
      EXPECT_EQ(ETCPAL_UUID_CMP(&lost_sources[i].cid, &lost_source_cids[i]), 0);
      EXPECT_EQ(strcmp(lost_sources[i].name, kTestName), 0);
      EXPECT_EQ(lost_sources[i].terminated, (i % 2) == 0);
    }

    EXPECT_EQ(num_lost_sources, kNumLostSources);
    EXPECT_EQ(context, &kTestContext);
  };

  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();

  EXPECT_EQ(get_expired_sources_fake.call_count, 1u);
  EXPECT_EQ(sources_lost_fake.call_count, 1u);
  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);
}

TEST_F(TestReceiverThread, SamplingPeriodStartedWorks)
{
  sampling_period_started_fake.custom_fake = [](sacn_receiver_t handle, uint16_t universe, void* context) {
    EXPECT_EQ(handle, kFirstReceiverHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(context, &kTestContext);
  };

  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();

  EXPECT_EQ(sampling_period_started_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, SamplingPeriodEndedWorks)
{
  sampling_period_ended_fake.custom_fake = [](sacn_receiver_t handle, uint16_t universe, void* context) {
    EXPECT_EQ(handle, kFirstReceiverHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(context, &kTestContext);
  };

  EXPECT_EQ(sampling_period_ended_fake.call_count, 0u);

  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle();

  EXPECT_EQ(sampling_period_ended_fake.call_count, 0u);

  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  EXPECT_EQ(sampling_period_ended_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, SamplingPeriodTransitionWorks)
{
  std::vector<SacnMcastInterface> initial_netints = {kTestNetints.begin(),
                                                     kTestNetints.begin() + (kTestNetints.size() / 2)};
  std::vector<SacnMcastInterface> future_netints = {kTestNetints.begin() + (kTestNetints.size() / 2),
                                                    kTestNetints.end()};

  etcpal_rbtree_clear_with_cb(&test_receiver_->sampling_period_netints, sampling_period_netint_tree_dealloc);
  for (const auto& netint : initial_netints)
    add_sacn_sampling_period_netint(&test_receiver_->sampling_period_netints, &netint.iface, false);
  for (const auto& netint : future_netints)
    add_sacn_sampling_period_netint(&test_receiver_->sampling_period_netints, &netint.iface, true);

  // Make sure sampling started is called after sampling ended
  sampling_period_ended_fake.custom_fake = [](sacn_receiver_t, uint16_t, void*) {
    EXPECT_EQ(sampling_period_started_fake.call_count, 0u);
  };
  sampling_period_started_fake.custom_fake = [](sacn_receiver_t, uint16_t, void*) {
    EXPECT_EQ(sampling_period_ended_fake.call_count, 1u);
  };

  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  EXPECT_EQ(sampling_period_ended_fake.call_count, 1u);
  EXPECT_EQ(sampling_period_started_fake.call_count, 1u);

  sampling_period_ended_fake.custom_fake = [](sacn_receiver_t, uint16_t, void*) {};
  sampling_period_started_fake.custom_fake = [](sacn_receiver_t, uint16_t, void*) {};

  for (const auto& netint : initial_netints)
    EXPECT_EQ(etcpal_rbtree_find(&test_receiver_->sampling_period_netints, &netint.iface), nullptr);
  for (const auto& netint : future_netints)
    EXPECT_NE(etcpal_rbtree_find(&test_receiver_->sampling_period_netints, &netint.iface), nullptr);

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sampling_period_netints), future_netints.size());

  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  for (SacnSamplingPeriodNetint* sp_netint = reinterpret_cast<SacnSamplingPeriodNetint*>(
           etcpal_rbiter_first(&iter, &test_receiver_->sampling_period_netints));
       sp_netint; sp_netint = reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_next(&iter)))
  {
    EXPECT_FALSE(sp_netint->in_future_sampling_period);
  }

  EXPECT_TRUE(test_receiver_->sampling);

  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  EXPECT_EQ(sampling_period_ended_fake.call_count, 2u);
  EXPECT_EQ(sampling_period_started_fake.call_count, 1u);

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sampling_period_netints), 0u);
}

TEST_F(TestReceiverThread, SourcePapLostWorks)
{
  source_pap_lost_fake.custom_fake = [](sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source,
                                        void* context) {
    EXPECT_EQ(handle, kFirstReceiverHandle);
    EXPECT_EQ(universe, kTestUniverse);
    EXPECT_EQ(ETCPAL_UUID_CMP(&source->cid, &kTestCid), 0);
    EXPECT_EQ(strcmp(source->name, kTestName), 0);
    EXPECT_EQ(context, &kTestContext);
  };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

  EXPECT_EQ(source_pap_lost_fake.call_count, 0u);

  etcpal_getms_fake.return_val += (SACN_SOURCE_LOSS_TIMEOUT + 1u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

#if SACN_ETC_PRIORITY_EXTENSION
  EXPECT_EQ(source_pap_lost_fake.call_count, 1u);
#else
  EXPECT_EQ(source_pap_lost_fake.call_count, 0u);
#endif
}

#if SACN_DYNAMIC_MEM
TEST_F(TestReceiverThread, SourceLimitExceededWorks)
{
  int configured_max = 10;
  int expected_max = 10;
  TestSourceLimitExceeded(configured_max, expected_max);
}
#else   // SACN_DYNAMIC_MEM
TEST_F(TestReceiverThread, SourceLimitExceededWorksWithLowerSourceCountMax)
{
  int configured_max = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE - 1;
  int expected_max = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE - 1;
  TestSourceLimitExceeded(configured_max, expected_max);
}

TEST_F(TestReceiverThread, SourceLimitExceededWorksWithHigherSourceCountMax)
{
  int configured_max = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE + 1;
  int expected_max = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE;
  TestSourceLimitExceeded(configured_max, expected_max);
}

TEST_F(TestReceiverThread, SourceLimitExceededWorksWithInfiniteSourceCountMax)
{
  int configured_max = SACN_RECEIVER_INFINITE_SOURCES;
  int expected_max = SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE;
  TestSourceLimitExceeded(configured_max, expected_max);
}
#endif  // SACN_DYNAMIC_MEM

TEST_F(TestReceiverThread, SeqNumFilteringWorks)
{
  EXPECT_EQ(universe_data_fake.call_count, 0u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 10u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 11u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 21u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 21u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 20u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 2u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 3u);
  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size(), 0u, kTestCid, 1u);
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 4u);
}

#if SACN_ETC_PRIORITY_EXTENSION

/* Tests to run if PAP is enabled in the configuration. */

TEST_F(TestReceiverThread, UniverseDataWaitsForPapAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  for (int wait = 200; wait <= SACN_WAIT_FOR_PRIORITY; wait += 200)
  {
    etcpal_getms_fake.return_val += 200u;
    RunThreadCycle();
    EXPECT_EQ(universe_data_fake.call_count, 0u);
  }

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_PRIORITY); };

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_DMX); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 2u);
}

TEST_F(TestReceiverThread, UniverseDataEventuallyStopsWaitingForPap)
{
  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_DMX); };

  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
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

TEST_F(TestReceiverThread, PapExpirationRemovesInternalSourceAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 1u);

  RemoveTestData();

  for (int ms = (SACN_PERIODIC_INTERVAL + 1); ms <= SACN_SOURCE_LOSS_TIMEOUT; ms += (SACN_PERIODIC_INTERVAL + 1))
  {
    etcpal_getms_fake.return_val += static_cast<uint32_t>(SACN_PERIODIC_INTERVAL + 1);

    RunThreadCycle();
    EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 1u);
  }

  etcpal_getms_fake.return_val += static_cast<uint32_t>(SACN_PERIODIC_INTERVAL + 1);

  RunThreadCycle();
  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);
  EXPECT_EQ(sources_lost_fake.call_count, 0u);
}

#else  // SACN_ETC_PRIORITY_EXTENSION

/* Tests to run if PAP is disabled in the configuration. */

TEST_F(TestReceiverThread, LevelDataAlwaysNotifiesAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  universe_data_fake.custom_fake = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData* universe_data,
                                      void*) { EXPECT_EQ(universe_data->start_code, SACN_STARTCODE_DMX); };

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, PapNeverNotifiesAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 0u);

  InitTestData(SACN_STARTCODE_DMX, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();
  EXPECT_EQ(universe_data_fake.call_count, 1u);
}

TEST_F(TestReceiverThread, PapCreatesNoInternalSourcesDuringSamplingPeriod)
{
  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);
}

TEST_F(TestReceiverThread, PapCreatesNoInternalSourcesAfterSamplingPeriod)
{
  RunThreadCycle();
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle();

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);

  InitTestData(SACN_STARTCODE_PRIORITY, kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  RunThreadCycle();

  EXPECT_EQ(etcpal_rbtree_size(&test_receiver_->sources), 0u);
}

#endif  // SACN_ETC_PRIORITY_EXTENSION
