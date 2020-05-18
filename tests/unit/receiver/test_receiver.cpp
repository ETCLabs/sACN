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

#include <limits>
#include "etcpal_mock/common.h"
#include "etcpal_mock/thread.h"
#include "etcpal/acn_rlp.h"
#include "etcpal/cpp/uuid.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/pdu.h"
#include "sacn/private/receiver.h"
#include "sacn/private/util.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiver TestReceiverDynamic
#define TestReceiverWithNetwork TestReceiverWithNetworkDynamic
#else
#define TestReceiver TestReceiverStatic
#define TestReceiverWithNetwork TestReceiverWithNetworkStatic
#endif

// Fake functions for sACN receiver callbacks.
FAKE_VOID_FUNC(handle_universe_data, sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*,
               void*);
FAKE_VOID_FUNC(handle_sources_lost, sacn_receiver_t, const SacnLostSource*, size_t, void*);
FAKE_VOID_FUNC(handle_source_pcp_lost, sacn_receiver_t, const SacnRemoteSource*, void*);
FAKE_VOID_FUNC(handle_sampling_ended, sacn_receiver_t, void*);
FAKE_VOID_FUNC(handle_source_limit_exceeded, sacn_receiver_t, void*);

class TestReceiver : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_receiver_deinit();
    sacn_mem_deinit();
  }
};

class TestReceiverWithNetwork : public TestReceiver
{
public:
  static std::function<void(void*)> sacn_receive_thread;
  static IntHandleManager socket_handle_mgr;
  static std::map<etcpal_socket_t, uint16_t> socket_to_universe;
  static etcpal::Uuid fixture_uuid;

protected:
  void SetUp() override
  {
    TestReceiver::SetUp();

    fixture_uuid = etcpal::Uuid::V4();

    init_int_handle_manager(&socket_handle_mgr, [](int handle_val) {
      return (socket_to_universe.find(static_cast<etcpal_socket_t>(handle_val)) != socket_to_universe.end());
    });

    etcpal_thread_create_fake.custom_fake = [](etcpal_thread_t* id, const EtcPalThreadParams* params,
                                               void (*thread_fn)(void*), void* thread_arg) {
      EXPECT_EQ(sacn_receive_thread, nullptr);
      sacn_receive_thread = thread_fn;
      return kEtcPalErrOk;
    };

    sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_iptype_t ip_type,
                                                   uint16_t universe, const SacnMcastNetintId* netints,
                                                   size_t num_netints, etcpal_socket_t* socket) {
      SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
      EXPECT_NE(context, nullptr);

      etcpal_socket_t new_socket = static_cast<etcpal_socket_t>(get_next_int_handle(&socket_handle_mgr));
      EXPECT_EQ(add_socket_ref(context, new_socket), true);
      socket_to_universe.insert(std::make_pair(new_socket, universe));

      *socket = new_socket;

      return kEtcPalErrOk;
    };

    sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t socket,
                                                      bool close_now) { socket_to_universe.erase(socket); };

    sacn_read_fake.custom_fake = [](SacnRecvThreadContext* recv_thread_context, SacnReadResult* read_result) {
      EXPECT_NE(recv_thread_context, nullptr);
      EXPECT_NE(read_result, nullptr);
      EXPECT_NE(recv_thread_context->socket_refs, nullptr);
      EXPECT_EQ(recv_thread_context->num_socket_refs, 1);

      // The socket should have a corresponding universe ID in socket_to_universe - grab it.
      auto universe_iter = socket_to_universe.find(recv_thread_context->socket_refs[0].sock);
      EXPECT_NE(universe_iter, socket_to_universe.end());
      const uint16_t kUniverse = universe_iter->second;

      // Pack recv_thread_context->recv_buf with fake network data based on the universe.
      char name_buffer[SACN_SOURCE_NAME_MAX_LEN];
      memset(name_buffer, '\0', SACN_SOURCE_NAME_MAX_LEN);
      int chars_written = snprintf(name_buffer, SACN_SOURCE_NAME_MAX_LEN, "Fake sACN Universe %u", kUniverse);

      const etcpal::Uuid kSourceCid =
          etcpal::Uuid::V5(fixture_uuid, name_buffer,
                           (chars_written < SACN_SOURCE_NAME_MAX_LEN) ? chars_written : SACN_SOURCE_NAME_MAX_LEN);

      const uint8_t kPriority = 100u;
      const bool kPreview = false;
      const uint8_t kStartCode = 0x00u;
      const uint16_t kSlotCount = 0x0200u;

      const size_t kDataHeaderLength =
          pack_sacn_data_header(recv_thread_context->recv_buf, &kSourceCid.get(), name_buffer, kPriority, kPreview,
                                kUniverse, kStartCode, kSlotCount);

      for (uint16_t slotIndex = 0; (slotIndex + 1) < kSlotCount; slotIndex += 2)
      {
        etcpal_pack_u16b(recv_thread_context->recv_buf + kDataHeaderLength + slotIndex, kUniverse);
      }

      read_result->data = recv_thread_context->recv_buf;
      read_result->data_len = kDataHeaderLength + kSlotCount;
      // TODO: Figure out what to do (if anything) with read_result->from_addr

      // Only let sacn_receive_thread run once so it doesn't block forever.
      recv_thread_context->running = false;

      return kEtcPalErrOk;
    };
  }

  void TearDown() override
  {
    sacn_receive_thread = nullptr;
    TestReceiver::TearDown();
  }
};

std::function<void(void*)> TestReceiverWithNetwork::sacn_receive_thread;
IntHandleManager TestReceiverWithNetwork::socket_handle_mgr;
std::map<etcpal_socket_t, uint16_t> TestReceiverWithNetwork::socket_to_universe;
etcpal::Uuid TestReceiverWithNetwork::fixture_uuid;

TEST_F(TestReceiver, SetStandardVersionWorks)
{
  // Initialization should set it to the default
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionAll);

  sacn_receiver_set_standard_version(kSacnStandardVersionDraft);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionDraft);
  sacn_receiver_set_standard_version(kSacnStandardVersionPublished);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionPublished);
  sacn_receiver_set_standard_version(kSacnStandardVersionAll);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionAll);
  sacn_receiver_set_standard_version(kSacnStandardVersionNone);
  EXPECT_EQ(sacn_receiver_get_standard_version(), kSacnStandardVersionNone);
}

TEST_F(TestReceiver, SetExpiredWaitWorks)
{
  // Initialization should set it to the default
  EXPECT_EQ(sacn_receiver_get_expired_wait(), SACN_DEFAULT_EXPIRED_WAIT_MS);

  sacn_receiver_set_expired_wait(0);
  EXPECT_EQ(sacn_receiver_get_expired_wait(), 0u);
  sacn_receiver_set_expired_wait(5000);
  EXPECT_EQ(sacn_receiver_get_expired_wait(), 5000u);
  sacn_receiver_set_expired_wait(std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(sacn_receiver_get_expired_wait(), std::numeric_limits<uint32_t>::max());
}

TEST_F(TestReceiverWithNetwork, Placeholder)
{
  SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
  config.callbacks = {handle_universe_data, handle_sources_lost, handle_source_pcp_lost, handle_sampling_ended,
                      handle_source_limit_exceeded};
  config.callback_context = this;
  config.universe_id = 1;  // Start at universe 1.

  sacn_receiver_t handle;
  sacn_receiver_create(&config, &handle);

  unsigned int num_threads = sacn_mem_get_num_threads();
  ASSERT_NE(num_threads, 0);
  EXPECT_EQ(num_threads, 1);

  ASSERT_NE(sacn_receive_thread, nullptr);
  sacn_receive_thread(get_recv_thread_context(0));

  sacn_receiver_destroy(handle);
}
