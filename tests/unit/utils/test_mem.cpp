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

#include "sacn/private/mem.h"

#include <string>
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "sacn/private/common.h"
#include "sacn/private/opts.h"
#include "sacn_mock/private/common.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestMem TestMemDynamic
#else
#define TestMem TestMemStatic
#endif

static constexpr sacn_merge_receiver_t kTestMergeReceiverHandle = 1;
static constexpr SacnMergeReceiverConfig kTestMergeReceiverConfig = {
    1u,
    {[](sacn_merge_receiver_t, const SacnRecvMergedData*, void*) {},
     [](sacn_merge_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*, const SacnRecvUniverseData*, void*) {},
     NULL, NULL},
    {1, DMX_ADDRESS_COUNT},
    SACN_RECEIVER_INFINITE_SOURCES,
    true,
    kSacnIpV4AndIpV6};

class TestMem : public ::testing::Test
{
protected:
  static constexpr unsigned int kTestNumThreads = 1;  // TODO: Set back to 4 if/when SACN_RECEIVER_MAX_THREADS increases
  static constexpr intptr_t kMagicPointerValue = 0xdeadbeef;

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    ASSERT_EQ(sacn_source_mem_init(), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_mem_init(kTestNumThreads), kEtcPalErrOk);
    ASSERT_EQ(sacn_merge_receiver_mem_init(kTestNumThreads), kEtcPalErrOk);
    ASSERT_EQ(sacn_source_detector_mem_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_source_detector_mem_deinit();
    sacn_merge_receiver_mem_deinit();
    sacn_receiver_mem_deinit();
    sacn_source_mem_deinit();
  }

  void DoForEachThread(std::function<void(sacn_thread_id_t)>&& fn)
  {
    for (sacn_thread_id_t thread = 0; thread < kTestNumThreads; ++thread)
    {
      SCOPED_TRACE("While testing thread ID " + std::to_string(thread));
      fn(thread);
    }
  }
};

TEST_F(TestMem, GetNumThreadsWorks)
{
  EXPECT_EQ(sacn_mem_get_num_threads(), kTestNumThreads);
}

TEST_F(TestMem, ValidInitializedStatusLists)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnSourceStatusLists* status_lists = get_status_lists(thread);
    ASSERT_NE(status_lists, nullptr);

    EXPECT_EQ(status_lists->num_online, 0u);
    EXPECT_EQ(status_lists->num_offline, 0u);
    EXPECT_EQ(status_lists->num_unknown, 0u);
  });
}

TEST_F(TestMem, StatusListsAreReZeroedWithEachGet)
{
  SacnSourceStatusLists* status_lists = get_status_lists(0);
  ASSERT_NE(status_lists, nullptr);

  // Modify some elements
  status_lists->num_online = 20;
  status_lists->num_offline = 40;
  status_lists->num_unknown = 60;

  // Now get again and make sure they are re-zeroed
  status_lists = get_status_lists(0);
  ASSERT_NE(status_lists, nullptr);

  EXPECT_EQ(status_lists->num_online, 0u);
  EXPECT_EQ(status_lists->num_offline, 0u);
  EXPECT_EQ(status_lists->num_unknown, 0u);
}

TEST_F(TestMem, StatusListsAddOfflineWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnSourceStatusLists* status_lists = get_status_lists(thread);
    ASSERT_NE(status_lists, nullptr);

    sacn_remote_source_t handle_to_add = 0u;

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_offline_source(status_lists, handle_to_add, test_name.c_str(), true));
      EXPECT_EQ(status_lists->num_offline, i + 1);
      EXPECT_EQ(status_lists->offline[i].handle, handle_to_add);
      EXPECT_STREQ(status_lists->offline[i].name, test_name.c_str());
      EXPECT_EQ(status_lists->offline[i].terminated, true);
      ++handle_to_add;
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
    {
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_offline_source(status_lists, handle_to_add, test_name.c_str(), true));
      EXPECT_EQ(status_lists->num_offline, i + 1);
      EXPECT_EQ(status_lists->offline[i].handle, handle_to_add);
      EXPECT_STREQ(status_lists->offline[i].name, test_name.c_str());
      EXPECT_EQ(status_lists->offline[i].terminated, true);
      ++handle_to_add;
    }
    // And make sure we can't add another
    EXPECT_FALSE(add_offline_source(status_lists, handle_to_add, "test name", true));
#endif
  });
}

TEST_F(TestMem, StatusListsAddOnlineWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnSourceStatusLists* status_lists = get_status_lists(thread);
    ASSERT_NE(status_lists, nullptr);

    sacn_remote_source_t handle_to_add = 0u;

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_online_source(status_lists, handle_to_add, test_name.c_str()));
      EXPECT_EQ(status_lists->num_online, i + 1);
      EXPECT_EQ(status_lists->online[i].handle, handle_to_add);
      EXPECT_STREQ(status_lists->online[i].name, test_name.c_str());
      ++handle_to_add;
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
    {
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_online_source(status_lists, handle_to_add, test_name.c_str()));
      EXPECT_EQ(status_lists->num_online, i + 1);
      EXPECT_EQ(status_lists->online[i].handle, handle_to_add);
      EXPECT_STREQ(status_lists->online[i].name, test_name.c_str());
      ++handle_to_add;
    }
    // And make sure we can't add another
    EXPECT_FALSE(add_online_source(status_lists, handle_to_add, "test name"));
#endif
  });
}

TEST_F(TestMem, StatusListsAddUnknownWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnSourceStatusLists* status_lists = get_status_lists(thread);
    ASSERT_NE(status_lists, nullptr);

    sacn_remote_source_t handle_to_add = 0u;

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_unknown_source(status_lists, handle_to_add, test_name.c_str()));
      EXPECT_EQ(status_lists->num_unknown, i + 1);
      EXPECT_EQ(status_lists->unknown[i].handle, handle_to_add);
      EXPECT_STREQ(status_lists->unknown[i].name, test_name.c_str());
      ++handle_to_add;
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
    {
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_unknown_source(status_lists, handle_to_add, test_name.c_str()));
      EXPECT_EQ(status_lists->num_unknown, i + 1);
      EXPECT_EQ(status_lists->unknown[i].handle, handle_to_add);
      EXPECT_STREQ(status_lists->unknown[i].name, test_name.c_str());
      ++handle_to_add;
    }
    // And make sure we can't add another
    EXPECT_FALSE(add_unknown_source(status_lists, handle_to_add, "test name"));
#endif
  });
}

TEST_F(TestMem, ValidInitializedToEraseBuffer)
{
  DoForEachThread([](sacn_thread_id_t thread) {
#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number for the buffer size
    SacnTrackedSource** to_erase_buf = get_to_erase_buffer(thread, 20);
    ASSERT_NE(to_erase_buf, nullptr);

    for (size_t i = 0; i < 20; ++i)
      EXPECT_EQ(to_erase_buf[i], nullptr);
#else
    // Just test some arbitrary number for the buffer size
    SacnTrackedSource** to_erase_buf = get_to_erase_buffer(thread, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE);
    ASSERT_NE(to_erase_buf, nullptr);

    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
      EXPECT_EQ(to_erase_buf[i], nullptr);

    // Trying to get more than the max capacity should not work
    to_erase_buf = get_to_erase_buffer(thread, SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE + 1);
    EXPECT_EQ(to_erase_buf, nullptr);
#endif
  });
}

TEST_F(TestMem, ToEraseIsReZeroedWithEachGet)
{
  SacnTrackedSource** to_erase = get_to_erase_buffer(0, 1);
  ASSERT_NE(to_erase, nullptr);

  // Modify some elements
  to_erase[0] = reinterpret_cast<SacnTrackedSource*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  to_erase = get_to_erase_buffer(0, 1);
  ASSERT_NE(to_erase, nullptr);

  EXPECT_EQ(to_erase[0], nullptr);
}

TEST_F(TestMem, ValidInitializedRecvThreadContext)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread);
    ASSERT_NE(recv_thread_context, nullptr);

    EXPECT_EQ(recv_thread_context->thread_id, thread);
    EXPECT_EQ(recv_thread_context->receivers, nullptr);
    EXPECT_EQ(recv_thread_context->num_receivers, 0u);

#if SACN_DYNAMIC_MEM
    EXPECT_NE(recv_thread_context->dead_sockets, nullptr);
    EXPECT_NE(recv_thread_context->socket_refs, nullptr);
#endif

    EXPECT_EQ(recv_thread_context->num_dead_sockets, 0u);
    EXPECT_EQ(recv_thread_context->num_socket_refs, 0u);
    EXPECT_EQ(recv_thread_context->new_socket_refs, 0u);
  });
}

TEST_F(TestMem, AddDeadSocketWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread);
    ASSERT_NE(recv_thread_context, nullptr);

    ReceiveSocket socket = RECV_SOCKET_DEFAULT_INIT;
#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      socket.handle = (etcpal_socket_t)i;
      ASSERT_TRUE(add_dead_socket(recv_thread_context, &socket));
      EXPECT_EQ(recv_thread_context->num_dead_sockets, i + 1);
      EXPECT_EQ(recv_thread_context->dead_sockets[i].handle, (etcpal_socket_t)i);
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_UNIVERSES * 2; ++i)
    {
      socket.handle = (etcpal_socket_t)i;
      ASSERT_TRUE(add_dead_socket(recv_thread_context, &socket));
      EXPECT_EQ(recv_thread_context->num_dead_sockets, i + 1);
      EXPECT_EQ(recv_thread_context->dead_sockets[i].handle, (etcpal_socket_t)i);
    }
    // And make sure we can't add another
    socket.handle = (etcpal_socket_t)SACN_RECEIVER_MAX_UNIVERSES;
    EXPECT_FALSE(add_dead_socket(recv_thread_context, &socket));
#endif
  });
}

TEST_F(TestMem, AddSocketRefWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread);
    ASSERT_NE(recv_thread_context, nullptr);

    ReceiveSocket new_socket = RECV_SOCKET_DEFAULT_INIT;

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      new_socket.handle = (etcpal_socket_t)i;
      ASSERT_NE(add_socket_ref(recv_thread_context, &new_socket), -1);
      EXPECT_EQ(recv_thread_context->num_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->new_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->socket_refs[i].socket.handle, (etcpal_socket_t)i);
      EXPECT_EQ(recv_thread_context->socket_refs[i].refcount, 1u);
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOCKET_REFS; ++i)
    {
      new_socket.handle = (etcpal_socket_t)i;
      ASSERT_NE(add_socket_ref(recv_thread_context, &new_socket), -1);
      EXPECT_EQ(recv_thread_context->num_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->new_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->socket_refs[i].socket.handle, (etcpal_socket_t)i);
      EXPECT_EQ(recv_thread_context->socket_refs[i].refcount, 1u);
    }
    // And make sure we can't add another
    new_socket.handle = (etcpal_socket_t)SACN_RECEIVER_MAX_SOCKET_REFS;
    EXPECT_EQ(add_socket_ref(recv_thread_context, &new_socket), -1);
#endif
  });
}

TEST_F(TestMem, RemoveSocketRefWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread);
    ASSERT_NE(recv_thread_context, nullptr);

    recv_thread_context->socket_refs[0] = SocketRef{{(etcpal_socket_t)0}, 1, true};
    recv_thread_context->socket_refs[1] = SocketRef{{(etcpal_socket_t)1}, 20, false};
    recv_thread_context->socket_refs[2] = SocketRef{{(etcpal_socket_t)2}, 3, false};
    recv_thread_context->num_socket_refs = 3;
    recv_thread_context->new_socket_refs = 1;

    // Remove the first socket ref (has a refcount of 1 and is pending), the other ones should be shifted
    ASSERT_TRUE(remove_socket_ref(recv_thread_context, 0));

    ASSERT_EQ(recv_thread_context->num_socket_refs, 2u);
    EXPECT_EQ(recv_thread_context->new_socket_refs, 0u);
    EXPECT_EQ(recv_thread_context->socket_refs[0].socket.handle, (etcpal_socket_t)1);
    EXPECT_EQ(recv_thread_context->socket_refs[0].refcount, 20u);
    EXPECT_FALSE(recv_thread_context->socket_refs[0].pending);
    EXPECT_EQ(recv_thread_context->socket_refs[1].socket.handle, (etcpal_socket_t)2);
    EXPECT_EQ(recv_thread_context->socket_refs[1].refcount, 3u);
    EXPECT_FALSE(recv_thread_context->socket_refs[1].pending);

    // Remove the last socket ref (multiple references), no shift should occur
    for (int i = 0; i < 2; ++i)
    {
      ASSERT_FALSE(remove_socket_ref(recv_thread_context, 1));  // (etcpal_socket_t)2

      ASSERT_EQ(recv_thread_context->num_socket_refs, 2u);
      EXPECT_EQ(recv_thread_context->new_socket_refs, 0u);
      EXPECT_EQ(recv_thread_context->socket_refs[0].socket.handle, (etcpal_socket_t)1);
      EXPECT_EQ(recv_thread_context->socket_refs[0].refcount, 20u);
      EXPECT_FALSE(recv_thread_context->socket_refs[0].pending);
      EXPECT_EQ(recv_thread_context->socket_refs[1].socket.handle, (etcpal_socket_t)2);
      EXPECT_EQ(recv_thread_context->socket_refs[1].refcount, 2u - i);
      EXPECT_FALSE(recv_thread_context->socket_refs[1].pending);
    }

    EXPECT_TRUE(remove_socket_ref(recv_thread_context, 1));
    ASSERT_EQ(recv_thread_context->num_socket_refs, 1u);
    EXPECT_EQ(recv_thread_context->new_socket_refs, 0u);
    EXPECT_EQ(recv_thread_context->socket_refs[0].socket.handle, (etcpal_socket_t)1);
    EXPECT_EQ(recv_thread_context->socket_refs[0].refcount, 20u);
    EXPECT_FALSE(recv_thread_context->socket_refs[0].pending);
  });
}

TEST_F(TestMem, ValidInitializedUniverseData)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    UniverseDataNotification* universe_data = get_universe_data(thread);
    ASSERT_NE(universe_data, nullptr);

    EXPECT_EQ(universe_data->api_callback, nullptr);
    EXPECT_EQ(universe_data->internal_callback, nullptr);
    EXPECT_EQ(universe_data->receiver_handle, SACN_RECEIVER_INVALID);
    EXPECT_EQ(universe_data->universe_data.values, nullptr);
    EXPECT_EQ(universe_data->thread_id, SACN_THREAD_ID_INVALID);
    EXPECT_EQ(universe_data->context, nullptr);
  });
}

TEST_F(TestMem, UniverseDataIsReZeroedWithEachGet)
{
  UniverseDataNotification* universe_data = get_universe_data(0);
  ASSERT_NE(universe_data, nullptr);

  // Modify some elements
  universe_data->receiver_handle = 2;
  universe_data->api_callback = reinterpret_cast<SacnUniverseDataCallback>(kMagicPointerValue);
  universe_data->internal_callback = reinterpret_cast<SacnUniverseDataInternalCallback>(kMagicPointerValue);
  universe_data->thread_id = (kTestNumThreads - 1u);
  universe_data->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  universe_data = get_universe_data(0);
  ASSERT_NE(universe_data, nullptr);

  EXPECT_EQ(universe_data->api_callback, nullptr);
  EXPECT_EQ(universe_data->internal_callback, nullptr);
  EXPECT_EQ(universe_data->receiver_handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(universe_data->universe_data.values, nullptr);
  EXPECT_EQ(universe_data->thread_id, SACN_THREAD_ID_INVALID);
  EXPECT_EQ(universe_data->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSourcesLostBuf)
{
  DoForEachThread([](sacn_thread_id_t thread) {
#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number for the buffer size
    SourcesLostNotification* sources_lost_buf = get_sources_lost_buffer(thread, 20);
    ASSERT_NE(sources_lost_buf, nullptr);

    for (int i = 0; i < 20; ++i)
    {
      auto sources_lost = &sources_lost_buf[i];
      EXPECT_EQ(sources_lost->api_callback, nullptr);
      EXPECT_EQ(sources_lost->internal_callback, nullptr);
      EXPECT_EQ(sources_lost->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sources_lost->num_lost_sources, 0u);
      EXPECT_EQ(sources_lost->thread_id, SACN_THREAD_ID_INVALID);
      EXPECT_EQ(sources_lost->context, nullptr);
    }
#else
    SourcesLostNotification* sources_lost_buf = get_sources_lost_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES);
    ASSERT_NE(sources_lost_buf, nullptr);

    // Test up to the maximum capacity
    for (int i = 0; i < SACN_RECEIVER_MAX_THREADS; ++i)
    {
      auto sources_lost = &sources_lost_buf[i];
      EXPECT_EQ(sources_lost->api_callback, nullptr);
      EXPECT_EQ(sources_lost->internal_callback, nullptr);
      EXPECT_EQ(sources_lost->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sources_lost->num_lost_sources, 0u);
      EXPECT_EQ(sources_lost->thread_id, SACN_THREAD_ID_INVALID);
      EXPECT_EQ(sources_lost->context, nullptr);
    }

    // Trying to get more than the max capacity should not work
    sources_lost_buf = get_sources_lost_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES + 1);
    EXPECT_EQ(sources_lost_buf, nullptr);
#endif
  });
}

TEST_F(TestMem, AddLostSourceWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SourcesLostNotification* sources_lost = get_sources_lost_buffer(thread, 1);
    ASSERT_NE(sources_lost, nullptr);

    size_t i = 0;
#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (; i < 20; ++i)
    {
      auto cid_to_add = etcpal::Uuid::V4();
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_lost_source(sources_lost, static_cast<sacn_remote_source_t>(i), &cid_to_add.get(),
                                  test_name.c_str(), true));
      EXPECT_EQ(sources_lost->num_lost_sources, i + 1);
      EXPECT_EQ(sources_lost->lost_sources[i].cid, cid_to_add);
      EXPECT_STREQ(sources_lost->lost_sources[i].name, test_name.c_str());
      EXPECT_EQ(sources_lost->lost_sources[i].terminated, true);
    }
#else
    // Test up to the maximum capacity
    for (; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
    {
      auto cid_to_add = etcpal::Uuid::V4();
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(add_lost_source(sources_lost, static_cast<sacn_remote_source_t>(i), &cid_to_add.get(),
                                  test_name.c_str(), true));
      EXPECT_EQ(sources_lost->num_lost_sources, i + 1);
      EXPECT_EQ(sources_lost->lost_sources[i].cid, cid_to_add);
      EXPECT_STREQ(sources_lost->lost_sources[i].name, test_name.c_str());
      EXPECT_EQ(sources_lost->lost_sources[i].terminated, true);
    }
    // And make sure we can't add another
    auto cid_to_add = etcpal::Uuid::V4();
    EXPECT_FALSE(
        add_lost_source(sources_lost, static_cast<sacn_remote_source_t>(i), &cid_to_add.get(), "test name", true));
#endif
  });
}

TEST_F(TestMem, SourcesLostIsReZeroedWithEachGet)
{
  SourcesLostNotification* sources_lost = get_sources_lost_buffer(0, 1);
  ASSERT_NE(sources_lost, nullptr);

  // Modify some elements
  sources_lost->handle = 2;
  sources_lost->api_callback = reinterpret_cast<SacnSourcesLostCallback>(kMagicPointerValue);
  sources_lost->internal_callback = reinterpret_cast<SacnSourcesLostInternalCallback>(kMagicPointerValue);
  sources_lost->num_lost_sources = 10;
  sources_lost->thread_id = (kTestNumThreads - 1u);
  sources_lost->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  sources_lost = get_sources_lost_buffer(0, 1);
  ASSERT_NE(sources_lost, nullptr);

  EXPECT_EQ(sources_lost->api_callback, nullptr);
  EXPECT_EQ(sources_lost->internal_callback, nullptr);
  EXPECT_EQ(sources_lost->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(sources_lost->num_lost_sources, 0u);
  EXPECT_EQ(sources_lost->thread_id, SACN_THREAD_ID_INVALID);
  EXPECT_EQ(sources_lost->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSourcePapLost)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SourcePapLostNotification* source_pap_lost = get_source_pap_lost(thread);
    ASSERT_NE(source_pap_lost, nullptr);

    EXPECT_EQ(source_pap_lost->api_callback, nullptr);
    EXPECT_EQ(source_pap_lost->internal_callback, nullptr);
    EXPECT_EQ(source_pap_lost->handle, SACN_RECEIVER_INVALID);
    EXPECT_EQ(source_pap_lost->thread_id, SACN_THREAD_ID_INVALID);
    EXPECT_EQ(source_pap_lost->context, nullptr);
  });
}

TEST_F(TestMem, SourcePapLostIsReZeroedWithEachGet)
{
  SourcePapLostNotification* source_pap_lost = get_source_pap_lost(0);
  ASSERT_NE(source_pap_lost, nullptr);

  // Modify some elements
  source_pap_lost->handle = 2;
  source_pap_lost->api_callback = reinterpret_cast<SacnSourcePapLostCallback>(kMagicPointerValue);
  source_pap_lost->internal_callback = reinterpret_cast<SacnSourcePapLostInternalCallback>(kMagicPointerValue);
  source_pap_lost->thread_id = (kTestNumThreads - 1u);
  source_pap_lost->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  source_pap_lost = get_source_pap_lost(0);
  ASSERT_NE(source_pap_lost, nullptr);

  EXPECT_EQ(source_pap_lost->api_callback, nullptr);
  EXPECT_EQ(source_pap_lost->internal_callback, nullptr);
  EXPECT_EQ(source_pap_lost->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(source_pap_lost->thread_id, SACN_THREAD_ID_INVALID);
  EXPECT_EQ(source_pap_lost->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSamplingStartedBuf)
{
  DoForEachThread([](sacn_thread_id_t thread) {
#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number for the buffer size
    SamplingStartedNotification* sampling_started_buf = get_sampling_started_buffer(thread, 20);
    ASSERT_NE(sampling_started_buf, nullptr);

    for (int i = 0; i < 20; ++i)
    {
      auto sampling_started = &sampling_started_buf[i];
      EXPECT_EQ(sampling_started->api_callback, nullptr);
      EXPECT_EQ(sampling_started->internal_callback, nullptr);
      EXPECT_EQ(sampling_started->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sampling_started->thread_id, SACN_THREAD_ID_INVALID);
      EXPECT_EQ(sampling_started->context, nullptr);
    }
#else
    SamplingStartedNotification* sampling_started_buf =
        get_sampling_started_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES);
    ASSERT_NE(sampling_started_buf, nullptr);

    // Test up to the maximum capacity
    for (int i = 0; i < SACN_RECEIVER_MAX_THREADS; ++i)
    {
      auto sampling_started = &sampling_started_buf[i];
      EXPECT_EQ(sampling_started->api_callback, nullptr);
      EXPECT_EQ(sampling_started->internal_callback, nullptr);
      EXPECT_EQ(sampling_started->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sampling_started->thread_id, SACN_THREAD_ID_INVALID);
      EXPECT_EQ(sampling_started->context, nullptr);
    }

    // Trying to get more than the max capacity should not work
    sampling_started_buf = get_sampling_started_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES + 1);
    EXPECT_EQ(sampling_started_buf, nullptr);
#endif
  });
}

TEST_F(TestMem, SamplingStartedIsReZeroedWithEachGet)
{
  SamplingStartedNotification* sampling_started = get_sampling_started_buffer(0, 1);
  ASSERT_NE(sampling_started, nullptr);

  // Modify some elements
  sampling_started->handle = 2;
  sampling_started->api_callback = reinterpret_cast<SacnSamplingPeriodStartedCallback>(kMagicPointerValue);
  sampling_started->internal_callback = reinterpret_cast<SacnSamplingPeriodStartedInternalCallback>(kMagicPointerValue);
  sampling_started->thread_id = (kTestNumThreads - 1u);
  sampling_started->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  sampling_started = get_sampling_started_buffer(0, 1);
  ASSERT_NE(sampling_started, nullptr);

  EXPECT_EQ(sampling_started->api_callback, nullptr);
  EXPECT_EQ(sampling_started->internal_callback, nullptr);
  EXPECT_EQ(sampling_started->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(sampling_started->thread_id, SACN_THREAD_ID_INVALID);
  EXPECT_EQ(sampling_started->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSamplingEndedBuf)
{
  DoForEachThread([](sacn_thread_id_t thread) {
#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number for the buffer size
    SamplingEndedNotification* sampling_ended_buf = get_sampling_ended_buffer(thread, 20);
    ASSERT_NE(sampling_ended_buf, nullptr);

    for (int i = 0; i < 20; ++i)
    {
      auto sampling_ended = &sampling_ended_buf[i];
      EXPECT_EQ(sampling_ended->api_callback, nullptr);
      EXPECT_EQ(sampling_ended->internal_callback, nullptr);
      EXPECT_EQ(sampling_ended->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sampling_ended->thread_id, SACN_THREAD_ID_INVALID);
      EXPECT_EQ(sampling_ended->context, nullptr);
    }
#else
    SamplingEndedNotification* sampling_ended_buf = get_sampling_ended_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES);
    ASSERT_NE(sampling_ended_buf, nullptr);

    // Test up to the maximum capacity
    for (int i = 0; i < SACN_RECEIVER_MAX_THREADS; ++i)
    {
      auto sampling_ended = &sampling_ended_buf[i];
      EXPECT_EQ(sampling_ended->api_callback, nullptr);
      EXPECT_EQ(sampling_ended->internal_callback, nullptr);
      EXPECT_EQ(sampling_ended->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sampling_ended->thread_id, SACN_THREAD_ID_INVALID);
      EXPECT_EQ(sampling_ended->context, nullptr);
    }

    // Trying to get more than the max capacity should not work
    sampling_ended_buf = get_sampling_ended_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES + 1);
    EXPECT_EQ(sampling_ended_buf, nullptr);
#endif
  });
}

TEST_F(TestMem, SamplingEndedIsReZeroedWithEachGet)
{
  SamplingEndedNotification* sampling_ended = get_sampling_ended_buffer(0, 1);
  ASSERT_NE(sampling_ended, nullptr);

  // Modify some elements
  sampling_ended->handle = 2;
  sampling_ended->api_callback = reinterpret_cast<SacnSamplingPeriodEndedCallback>(kMagicPointerValue);
  sampling_ended->internal_callback = reinterpret_cast<SacnSamplingPeriodEndedInternalCallback>(kMagicPointerValue);
  sampling_ended->thread_id = (kTestNumThreads - 1u);
  sampling_ended->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  sampling_ended = get_sampling_ended_buffer(0, 1);
  ASSERT_NE(sampling_ended, nullptr);

  EXPECT_EQ(sampling_ended->api_callback, nullptr);
  EXPECT_EQ(sampling_ended->internal_callback, nullptr);
  EXPECT_EQ(sampling_ended->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(sampling_ended->thread_id, SACN_THREAD_ID_INVALID);
  EXPECT_EQ(sampling_ended->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSourceLimitExceeded)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SourceLimitExceededNotification* source_limit_exceeded = get_source_limit_exceeded(thread);
    ASSERT_NE(source_limit_exceeded, nullptr);

    EXPECT_EQ(source_limit_exceeded->api_callback, nullptr);
    EXPECT_EQ(source_limit_exceeded->internal_callback, nullptr);
    EXPECT_EQ(source_limit_exceeded->handle, SACN_RECEIVER_INVALID);
    EXPECT_EQ(source_limit_exceeded->thread_id, SACN_THREAD_ID_INVALID);
    EXPECT_EQ(source_limit_exceeded->context, nullptr);
  });
}

TEST_F(TestMem, SourceLimitExceededIsReZeroedWithEachGet)
{
  SourceLimitExceededNotification* source_limit_exceeded = get_source_limit_exceeded(0);
  ASSERT_NE(source_limit_exceeded, nullptr);

  // Modify some elements
  source_limit_exceeded->handle = 2;
  source_limit_exceeded->api_callback = reinterpret_cast<SacnSourceLimitExceededCallback>(kMagicPointerValue);
  source_limit_exceeded->internal_callback =
      reinterpret_cast<SacnSourceLimitExceededInternalCallback>(kMagicPointerValue);
  source_limit_exceeded->thread_id = (kTestNumThreads - 1u);
  source_limit_exceeded->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  source_limit_exceeded = get_source_limit_exceeded(0);
  ASSERT_NE(source_limit_exceeded, nullptr);

  EXPECT_EQ(source_limit_exceeded->api_callback, nullptr);
  EXPECT_EQ(source_limit_exceeded->internal_callback, nullptr);
  EXPECT_EQ(source_limit_exceeded->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(source_limit_exceeded->thread_id, SACN_THREAD_ID_INVALID);
  EXPECT_EQ(source_limit_exceeded->context, nullptr);
}

TEST_F(TestMem, AddReceiverToListWorks)
{
  SacnRecvThreadContext rtc{};
#if SACN_DYNAMIC_MEM
  SacnReceiver receiver{};
#else
  SacnReceiver receiver{{}, {}, {}, {}};   // Fixes error C3852
#endif

  add_receiver_to_list(&rtc, &receiver);
  ASSERT_EQ(rtc.receivers, &receiver);
  EXPECT_EQ(rtc.receivers->next, nullptr);
  EXPECT_EQ(rtc.num_receivers, 1u);

#if SACN_DYNAMIC_MEM
  SacnReceiver receiver2{};
#else
  SacnReceiver receiver2{{}, {}, {}, {}};  // Fixes error C3852
#endif
  add_receiver_to_list(&rtc, &receiver2);
  ASSERT_EQ(rtc.receivers, &receiver);
  ASSERT_EQ(rtc.receivers->next, &receiver2);
  EXPECT_EQ(rtc.receivers->next->next, nullptr);
  EXPECT_EQ(rtc.num_receivers, 2u);
}

TEST_F(TestMem, RemoveReceiverFromListWorks)
{
  SacnRecvThreadContext rtc{};
#if SACN_DYNAMIC_MEM
  SacnReceiver receiver{};
  SacnReceiver receiver2{};
  SacnReceiver receiver3{};
#else
  SacnReceiver receiver{{}, {}, {}, {}};   // Fixes error C3852
  SacnReceiver receiver2{{}, {}, {}, {}};
  SacnReceiver receiver3{{}, {}, {}, {}};
#endif

  rtc.receivers = &receiver;
  receiver.next = &receiver2;
  receiver2.next = &receiver3;

  rtc.num_receivers = 3;

  // Remove from the middle
  remove_receiver_from_list(&rtc, &receiver2);
  ASSERT_EQ(rtc.receivers, &receiver);
  ASSERT_EQ(rtc.receivers->next, &receiver3);
  EXPECT_EQ(rtc.receivers->next->next, nullptr);
  EXPECT_EQ(rtc.num_receivers, 2u);
  EXPECT_EQ(receiver2.next, nullptr);

  // Remove from the head
  remove_receiver_from_list(&rtc, &receiver);
  ASSERT_EQ(rtc.receivers, &receiver3);
  EXPECT_EQ(rtc.receivers->next, nullptr);
  EXPECT_EQ(rtc.num_receivers, 1u);
  EXPECT_EQ(receiver.next, nullptr);
}

TEST_F(TestMem, AddSacnMergeReceiverWorks)
{
  SacnMergeReceiver* merge_receiver = nullptr;
  EXPECT_EQ(add_sacn_merge_receiver(kTestMergeReceiverHandle, &kTestMergeReceiverConfig, &merge_receiver),
            kEtcPalErrOk);

  ASSERT_NE(merge_receiver, nullptr);
  EXPECT_EQ(merge_receiver->merge_receiver_handle, kTestMergeReceiverHandle);
  EXPECT_EQ(merge_receiver->merger_handle, SACN_DMX_MERGER_INVALID);
  EXPECT_EQ(merge_receiver->callbacks.universe_data, kTestMergeReceiverConfig.callbacks.universe_data);
  EXPECT_EQ(merge_receiver->callbacks.universe_non_dmx, kTestMergeReceiverConfig.callbacks.universe_non_dmx);
  EXPECT_EQ(merge_receiver->callbacks.source_limit_exceeded, nullptr);
}

TEST_F(TestMem, AddSacnMergeReceiverSourceWorks)
{
  static constexpr size_t kNumSources = 5u;

  SacnMergeReceiver* merge_receiver = nullptr;
  EXPECT_EQ(add_sacn_merge_receiver(kTestMergeReceiverHandle, &kTestMergeReceiverConfig, &merge_receiver),
            kEtcPalErrOk);

  EtcPalSockAddr source_addr;
  SacnRemoteSource source_info;
  etcpal::Uuid last_cid;
  for (size_t i = 0u; i < kNumSources; ++i)
  {
    EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), i);
    last_cid = etcpal::Uuid::V4();
    source_info.handle = static_cast<sacn_remote_source_t>(i);
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, &source_addr, &source_info, false), kEtcPalErrOk);
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources);

  source_info.handle = static_cast<sacn_remote_source_t>(kNumSources - 1u);
  EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, &source_addr, &source_info, false), kEtcPalErrExists);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources);
}

TEST_F(TestMem, RemoveSacnMergeReceiverSourceWorks)
{
  static constexpr size_t kNumSources = 5u;

  SacnMergeReceiver* merge_receiver = nullptr;
  EXPECT_EQ(add_sacn_merge_receiver(kTestMergeReceiverHandle, &kTestMergeReceiverConfig, &merge_receiver),
            kEtcPalErrOk);

  EtcPalSockAddr source_addr;
  SacnRemoteSource source_info;
  for (size_t i = 0u; i < kNumSources; ++i)
  {
    source_info.handle = static_cast<sacn_remote_source_t>(i);
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, &source_addr, &source_info, false), kEtcPalErrOk);
  }

  for (size_t i = 0u; i < kNumSources; ++i)
  {
    EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources - i);

    remove_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_remote_source_t>(i));
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
}

TEST_F(TestMem, InitCleansUpRecvThreadContext)
{
  SacnRecvThreadContext* context = get_recv_thread_context(0);
  context->running = true;
  context->num_dead_sockets = 3u;
  context->num_socket_refs = 3u;
  context->new_socket_refs = 3u;
  context->periodic_timer_started = true;

  sacn_receiver_mem_deinit();
  EXPECT_EQ(sacn_receiver_mem_init(kTestNumThreads), kEtcPalErrOk);

  context = get_recv_thread_context(0);
  EXPECT_FALSE(context->running);
  EXPECT_EQ(context->num_dead_sockets, 0u);
  EXPECT_EQ(context->num_socket_refs, 0u);
  EXPECT_EQ(context->new_socket_refs, 0u);
  EXPECT_FALSE(context->periodic_timer_started);
}

TEST_F(TestMem, RespectsMaxMergeReceiverLimit)
{
  SacnMergeReceiverConfig config = SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT;
  SacnMergeReceiver* merge_receiver = NULL;
  for (int i = 0; i < SACN_RECEIVER_MAX_UNIVERSES; ++i)
  {
    config.universe_id = static_cast<uint16_t>(i + 1);
    EXPECT_EQ(add_sacn_merge_receiver(static_cast<sacn_merge_receiver_t>(i), &config, &merge_receiver), kEtcPalErrOk);
  }
}

TEST_F(TestMem, RespectsMaxMergeReceiverSourceLimit)
{
  SacnMergeReceiverConfig config = SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT;

  SacnMergeReceiver* merge_receiver = NULL;
  EXPECT_EQ(add_sacn_merge_receiver(kTestMergeReceiverHandle, &config, &merge_receiver), kEtcPalErrOk);

  EtcPalSockAddr source_addr;
  SacnRemoteSource source_info;
  for (int i = 0; i < SACN_RECEIVER_TOTAL_MAX_SOURCES; ++i)
  {
    source_info.handle = static_cast<sacn_remote_source_t>(i);
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, &source_addr, &source_info, false), kEtcPalErrOk);
  }
}

TEST_F(TestMem, RespectsMaxRemoteSourceLimit)
{
  for (int i = 0; i < (SACN_RECEIVER_TOTAL_MAX_SOURCES + SACN_SOURCE_DETECTOR_MAX_SOURCES); ++i)
  {
    EtcPalUuid cid = etcpal::Uuid::V4().get();

    sacn_remote_source_t handle;
    EXPECT_EQ(add_remote_source_handle(&cid, &handle), kEtcPalErrOk);
  }
}

TEST_F(TestMem, RespectsMaxSourceDetectorSourceLimit)
{
  SacnUniverseDiscoverySource* state = nullptr;
  for (int i = 0; i < SACN_SOURCE_DETECTOR_MAX_SOURCES; ++i)
  {
    EtcPalUuid cid = etcpal::Uuid::V4().get();
    EXPECT_EQ(add_sacn_universe_discovery_source(&cid, "name", &state), kEtcPalErrOk);
  }
}
