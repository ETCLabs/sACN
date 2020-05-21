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

#include <optional>
#include "etcpal_mock/common.h"
#include "etcpal_mock/thread.h"
#include "etcpal/cpp/uuid.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/pdu.h"
#include "sacn/private/receiver.h"
#include "sacn/private/util.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiverWithNetwork TestReceiverWithNetworkDynamic
#else
#define TestReceiverWithNetwork TestReceiverWithNetworkStatic
#endif

// Fake functions for sACN receiver callbacks.
FAKE_VOID_FUNC(handle_universe_data, sacn_receiver_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*,
               void*);
FAKE_VOID_FUNC(handle_sources_lost, sacn_receiver_t, const SacnLostSource*, size_t, void*);
FAKE_VOID_FUNC(handle_source_pcp_lost, sacn_receiver_t, const SacnRemoteSource*, void*);
FAKE_VOID_FUNC(handle_sampling_ended, sacn_receiver_t, void*);
FAKE_VOID_FUNC(handle_source_limit_exceeded, sacn_receiver_t, void*);

class TestReceiverWithNetwork : public ::testing::Test
{
public:
  static const uint8_t kPriority;
  static const uint16_t kSlotCount;

  std::optional<uint16_t> GetUniverse(sacn_receiver_t handle)
  {
    auto universe_iter = handle_to_universe.find(handle);
    EXPECT_NE(universe_iter, handle_to_universe.end());

    if (universe_iter == handle_to_universe.end())
    {
      return std::nullopt;
    }

    return universe_iter->second;
  }

  static std::function<void(void*)> sacn_receive_thread;
  static IntHandleManager socket_handle_mgr;
  static std::map<etcpal_socket_t, uint16_t> socket_to_universe;
  static etcpal::Uuid fixture_uuid;

protected:
  const SacnReceiverConfig kDefaultReceiverConfig = {1,
                                                     {handle_universe_data, handle_sources_lost, handle_source_pcp_lost,
                                                      handle_sampling_ended, handle_source_limit_exceeded},
                                                     0,
                                                     this,
                                                     NULL,
                                                     0};

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_reset_all_fakes();

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_init(), kEtcPalErrOk);

    fixture_uuid = etcpal::Uuid::V4();

    init_int_handle_manager(&socket_handle_mgr, [](int handle_val) {
      return (socket_to_universe.find(static_cast<etcpal_socket_t>(handle_val)) != socket_to_universe.end());
    });

    etcpal_thread_create_fake.custom_fake = [](etcpal_thread_t* id, const EtcPalThreadParams* params,
                                               void (*thread_fn)(void*), void* thread_arg) {
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
                                                      bool close_now) {
      SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
      EXPECT_NE(context, nullptr);

      EXPECT_EQ(remove_socket_ref(context, socket), true);
      socket_to_universe.erase(socket);
    };

    sacn_read_fake.custom_fake = [](SacnRecvThreadContext* recv_thread_context, SacnReadResult* read_result) {
      EXPECT_NE(recv_thread_context, nullptr);
      EXPECT_NE(read_result, nullptr);
      EXPECT_NE(recv_thread_context->socket_refs, nullptr);
      EXPECT_EQ(recv_thread_context->num_socket_refs, 1u);

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

      const bool kPreview = false;
      const uint8_t kStartCode = SACN_STARTCODE_DMX;

      const size_t kDataHeaderLength =
          pack_sacn_data_header(recv_thread_context->recv_buf, &kSourceCid.get(), name_buffer, kPriority, kPreview,
                                kUniverse, kStartCode, kSlotCount);

      for (uint16_t slotIndex = 0; (slotIndex + 1) < kSlotCount; slotIndex += 2)
      {
        etcpal_pack_u16b(recv_thread_context->recv_buf + kDataHeaderLength + slotIndex, kUniverse);
      }

      read_result->data = recv_thread_context->recv_buf;
      read_result->data_len = kDataHeaderLength + kSlotCount;

      // Only let sacn_receive_thread run once so it doesn't block forever.
      recv_thread_context->running = false;

      return kEtcPalErrOk;
    };

    handle_universe_data_fake.custom_fake = [](sacn_receiver_t handle, const EtcPalSockAddr* from_addr,
                                               const SacnHeaderData* header, const uint8_t* pdata, void* context) {
      ASSERT_NE(from_addr, nullptr);
      ASSERT_NE(header, nullptr);
      ASSERT_NE(pdata, nullptr);
      ASSERT_NE(context, nullptr);

      TestReceiverWithNetwork* fixture = static_cast<TestReceiverWithNetwork*>(context);

      std::string source_name(header->source_name, SACN_SOURCE_NAME_MAX_LEN);
      source_name.erase(std::find(source_name.begin(), source_name.end(), '\0'), source_name.end());
      EXPECT_EQ(header->cid, etcpal::Uuid::V5(fixture_uuid, source_name.c_str(), source_name.length()));

      auto universe_opt = fixture->GetUniverse(handle);
      EXPECT_EQ(universe_opt.has_value(), true);
      const uint16_t kUniverse = universe_opt.value();
      EXPECT_EQ(header->universe_id, kUniverse);

      EXPECT_EQ(header->priority, kPriority);
      EXPECT_EQ(header->preview, false);
      EXPECT_EQ(header->start_code, SACN_STARTCODE_DMX);
      EXPECT_EQ(header->slot_count, kSlotCount);

      for (uint16_t slotIndex = 0; (slotIndex + 1) < kSlotCount; slotIndex += 2)
      {
        EXPECT_EQ(etcpal_unpack_u16b(pdata + slotIndex), kUniverse);
      }
    };
  }

  void TearDown() override
  {
    etcpal_thread_create_fake.custom_fake = nullptr;
    sacn_add_receiver_socket_fake.custom_fake = nullptr;
    sacn_remove_receiver_socket_fake.custom_fake = nullptr;
    sacn_read_fake.custom_fake = nullptr;
    handle_universe_data_fake.custom_fake = nullptr;

    sacn_receive_thread = nullptr;
    socket_to_universe.clear();
    handle_to_universe.clear();
    fixture_uuid = etcpal::Uuid();

    sacn_receiver_deinit();
    sacn_mem_deinit();
  }

  virtual void ExpectUniverse(sacn_receiver_t handle, uint16_t universe) { handle_to_universe[handle] = universe; }

  std::map<sacn_receiver_t, uint16_t> handle_to_universe;
};

std::function<void(void*)> TestReceiverWithNetwork::sacn_receive_thread;
IntHandleManager TestReceiverWithNetwork::socket_handle_mgr;
std::map<etcpal_socket_t, uint16_t> TestReceiverWithNetwork::socket_to_universe;
etcpal::Uuid TestReceiverWithNetwork::fixture_uuid;
const uint8_t TestReceiverWithNetwork::kPriority = 100u;
const uint16_t TestReceiverWithNetwork::kSlotCount = 0x0200u;

TEST_F(TestReceiverWithNetwork, CreateAndDestroyWork)
{
  SacnReceiverConfig config = kDefaultReceiverConfig;

  for (uint16_t universe = 1u; universe < 0x8000u; universe *= 2u)
  {
    config.universe_id = universe;

    sacn_receiver_t handle;
    sacn_receiver_create(&config, &handle);
    ExpectUniverse(handle, universe);

    unsigned int num_threads = sacn_mem_get_num_threads();
    ASSERT_NE(num_threads, 0u);
    EXPECT_EQ(num_threads, 1u);
    ASSERT_NE(sacn_receive_thread, nullptr);

    sacn_receive_thread(get_recv_thread_context(0));

    sacn_receiver_destroy(handle);
  }
}

TEST_F(TestReceiverWithNetwork, ChangeUniverseWorks)
{
  SacnReceiverConfig config = kDefaultReceiverConfig;
  sacn_receiver_t handle;

  for (uint16_t universe = 1u; universe < 0x8000u; universe *= 2u)
  {
    if (universe == 1u)
    {
      config.universe_id = universe;
      sacn_receiver_create(&config, &handle);
    }
    else
    {
      sacn_receiver_change_universe(handle, universe);
      get_recv_thread_context(0)->running = true;  // The sacn_read custom fake sets this to false - reset to true.
    }

    ExpectUniverse(handle, universe);

    unsigned int num_threads = sacn_mem_get_num_threads();
    ASSERT_NE(num_threads, 0u);
    EXPECT_EQ(num_threads, 1u);
    ASSERT_NE(sacn_receive_thread, nullptr);

    sacn_receive_thread(get_recv_thread_context(0));
  }

  sacn_receiver_destroy(handle);
}
