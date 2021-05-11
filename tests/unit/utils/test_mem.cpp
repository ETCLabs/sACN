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
    {[](sacn_merge_receiver_t, uint16_t, const uint8_t*, const sacn_remote_source_t*, void*) {},
     [](sacn_merge_receiver_t, uint16_t, const EtcPalSockAddr*, const SacnHeaderData*, const uint8_t*, void*) {}, NULL,
     NULL},
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
    ASSERT_EQ(sacn_mem_init(kTestNumThreads), kEtcPalErrOk);
  }

  void TearDown() override { sacn_mem_deinit(); }

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

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      ASSERT_TRUE(add_dead_socket(recv_thread_context, (etcpal_socket_t)i));
      EXPECT_EQ(recv_thread_context->num_dead_sockets, i + 1);
      EXPECT_EQ(recv_thread_context->dead_sockets[i], (etcpal_socket_t)i);
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_UNIVERSES * 2; ++i)
    {
      ASSERT_TRUE(add_dead_socket(recv_thread_context, (etcpal_socket_t)i));
      EXPECT_EQ(recv_thread_context->num_dead_sockets, i + 1);
      EXPECT_EQ(recv_thread_context->dead_sockets[i], (etcpal_socket_t)i);
    }
    // And make sure we can't add another
    EXPECT_FALSE(add_dead_socket(recv_thread_context, (etcpal_socket_t)SACN_RECEIVER_MAX_UNIVERSES));
#endif
  });
}

TEST_F(TestMem, AddSocketRefWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread);
    ASSERT_NE(recv_thread_context, nullptr);

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      ASSERT_TRUE(add_socket_ref(recv_thread_context, (etcpal_socket_t)i, kEtcPalIpTypeInvalid, false));
      EXPECT_EQ(recv_thread_context->num_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->new_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->socket_refs[i].sock, (etcpal_socket_t)i);
      EXPECT_EQ(recv_thread_context->socket_refs[i].refcount, 1u);
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOCKET_REFS; ++i)
    {
      ASSERT_TRUE(add_socket_ref(recv_thread_context, (etcpal_socket_t)i, kEtcPalIpTypeInvalid, false));
      EXPECT_EQ(recv_thread_context->num_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->new_socket_refs, i + 1);
      EXPECT_EQ(recv_thread_context->socket_refs[i].sock, (etcpal_socket_t)i);
      EXPECT_EQ(recv_thread_context->socket_refs[i].refcount, 1u);
    }
    // And make sure we can't add another
    EXPECT_FALSE(add_socket_ref(recv_thread_context, (etcpal_socket_t)SACN_RECEIVER_MAX_SOCKET_REFS,
                                kEtcPalIpTypeInvalid, false));
#endif
  });
}

TEST_F(TestMem, RemoveSocketRefWorks)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread);
    ASSERT_NE(recv_thread_context, nullptr);

    recv_thread_context->socket_refs[0] = SocketRef{(etcpal_socket_t)0, 1};
    recv_thread_context->socket_refs[1] = SocketRef{(etcpal_socket_t)1, 20};
    recv_thread_context->socket_refs[2] = SocketRef{(etcpal_socket_t)2, 3};
    recv_thread_context->num_socket_refs = 3;
    recv_thread_context->new_socket_refs = 1;

    // Remove a socket ref that has a refcount of 1, the other ones should be shifted
    ASSERT_TRUE(remove_socket_ref(recv_thread_context, (etcpal_socket_t)0));

    ASSERT_EQ(recv_thread_context->num_socket_refs, 2u);
    EXPECT_EQ(recv_thread_context->new_socket_refs, 1u);
    EXPECT_EQ(recv_thread_context->socket_refs[0].sock, (etcpal_socket_t)1);
    EXPECT_EQ(recv_thread_context->socket_refs[0].refcount, 20u);
    EXPECT_EQ(recv_thread_context->socket_refs[1].sock, (etcpal_socket_t)2);
    EXPECT_EQ(recv_thread_context->socket_refs[1].refcount, 3u);

    // Remove one with multiple references
    for (int i = 0; i < 2; ++i)
      ASSERT_FALSE(remove_socket_ref(recv_thread_context, (etcpal_socket_t)2));
    EXPECT_TRUE(remove_socket_ref(recv_thread_context, (etcpal_socket_t)2));
    EXPECT_EQ(recv_thread_context->num_socket_refs, 1u);
  });
}

TEST_F(TestMem, ValidInitializedUniverseData)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    UniverseDataNotification* universe_data = get_universe_data(thread);
    ASSERT_NE(universe_data, nullptr);

    EXPECT_EQ(universe_data->callback, nullptr);
    EXPECT_EQ(universe_data->receiver_handle, SACN_RECEIVER_INVALID);
    EXPECT_EQ(universe_data->pdata, nullptr);
    EXPECT_EQ(universe_data->context, nullptr);
  });
}

TEST_F(TestMem, UniverseDataIsReZeroedWithEachGet)
{
  UniverseDataNotification* universe_data = get_universe_data(0);
  ASSERT_NE(universe_data, nullptr);

  // Modify some elements
  universe_data->receiver_handle = 2;
  universe_data->callback = reinterpret_cast<SacnUniverseDataCallback>(kMagicPointerValue);
  universe_data->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  universe_data = get_universe_data(0);
  ASSERT_NE(universe_data, nullptr);

  EXPECT_EQ(universe_data->callback, nullptr);
  EXPECT_EQ(universe_data->receiver_handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(universe_data->pdata, nullptr);
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
      EXPECT_EQ(sources_lost->callback, nullptr);
      EXPECT_EQ(sources_lost->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sources_lost->num_lost_sources, 0u);
      EXPECT_EQ(sources_lost->context, nullptr);
    }
#else
    SourcesLostNotification* sources_lost_buf = get_sources_lost_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES);
    ASSERT_NE(sources_lost_buf, nullptr);

    // Test up to the maximum capacity
    for (int i = 0; i < SACN_RECEIVER_MAX_THREADS; ++i)
    {
      auto sources_lost = &sources_lost_buf[i];
      EXPECT_EQ(sources_lost->callback, nullptr);
      EXPECT_EQ(sources_lost->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sources_lost->num_lost_sources, 0u);
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

#if SACN_DYNAMIC_MEM
    // Just test some arbitrary number
    for (size_t i = 0; i < 20; ++i)
    {
      auto cid_to_add = etcpal::Uuid::V4();
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(
          add_lost_source(sources_lost, SACN_REMOTE_SOURCE_INVALID, &cid_to_add.get(), test_name.c_str(), true));
      EXPECT_EQ(sources_lost->num_lost_sources, i + 1);
      EXPECT_EQ(sources_lost->lost_sources[i].cid, cid_to_add);
      EXPECT_STREQ(sources_lost->lost_sources[i].name, test_name.c_str());
      EXPECT_EQ(sources_lost->lost_sources[i].terminated, true);
    }
#else
    // Test up to the maximum capacity
    for (size_t i = 0; i < SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE; ++i)
    {
      auto cid_to_add = etcpal::Uuid::V4();
      std::string test_name = "test name " + std::to_string(i);
      ASSERT_TRUE(
          add_lost_source(sources_lost, SACN_REMOTE_SOURCE_INVALID, &cid_to_add.get(), test_name.c_str(), true));
      EXPECT_EQ(sources_lost->num_lost_sources, i + 1);
      EXPECT_EQ(sources_lost->lost_sources[i].cid, cid_to_add);
      EXPECT_STREQ(sources_lost->lost_sources[i].name, test_name.c_str());
      EXPECT_EQ(sources_lost->lost_sources[i].terminated, true);
    }
    // And make sure we can't add another
    auto cid_to_add = etcpal::Uuid::V4();
    EXPECT_FALSE(add_lost_source(sources_lost, SACN_REMOTE_SOURCE_INVALID, &cid_to_add.get(), "test name", true));
#endif
  });
}

TEST_F(TestMem, SourcesLostIsReZeroedWithEachGet)
{
  SourcesLostNotification* sources_lost = get_sources_lost_buffer(0, 1);
  ASSERT_NE(sources_lost, nullptr);

  // Modify some elements
  sources_lost->handle = 2;
  sources_lost->callback = reinterpret_cast<SacnSourcesLostCallback>(kMagicPointerValue);
  sources_lost->num_lost_sources = 10;
  sources_lost->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  sources_lost = get_sources_lost_buffer(0, 1);
  ASSERT_NE(sources_lost, nullptr);

  EXPECT_EQ(sources_lost->callback, nullptr);
  EXPECT_EQ(sources_lost->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(sources_lost->num_lost_sources, 0u);
  EXPECT_EQ(sources_lost->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSourcePapLost)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SourcePapLostNotification* source_pap_lost = get_source_pap_lost(thread);
    ASSERT_NE(source_pap_lost, nullptr);

    EXPECT_EQ(source_pap_lost->callback, nullptr);
    EXPECT_EQ(source_pap_lost->handle, SACN_RECEIVER_INVALID);
    EXPECT_EQ(source_pap_lost->context, nullptr);
  });
}

TEST_F(TestMem, SourcePapLostIsReZeroedWithEachGet)
{
  SourcePapLostNotification* source_pap_lost = get_source_pap_lost(0);
  ASSERT_NE(source_pap_lost, nullptr);

  // Modify some elements
  source_pap_lost->handle = 2;
  source_pap_lost->callback = reinterpret_cast<SacnSourcePapLostCallback>(kMagicPointerValue);
  source_pap_lost->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  source_pap_lost = get_source_pap_lost(0);
  ASSERT_NE(source_pap_lost, nullptr);

  EXPECT_EQ(source_pap_lost->callback, nullptr);
  EXPECT_EQ(source_pap_lost->handle, SACN_RECEIVER_INVALID);
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
      EXPECT_EQ(sampling_started->callback, nullptr);
      EXPECT_EQ(sampling_started->handle, SACN_RECEIVER_INVALID);
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
      EXPECT_EQ(sampling_started->callback, nullptr);
      EXPECT_EQ(sampling_started->handle, SACN_RECEIVER_INVALID);
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
  sampling_started->callback = reinterpret_cast<SacnSamplingPeriodStartedCallback>(kMagicPointerValue);
  sampling_started->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  sampling_started = get_sampling_started_buffer(0, 1);
  ASSERT_NE(sampling_started, nullptr);

  EXPECT_EQ(sampling_started->callback, nullptr);
  EXPECT_EQ(sampling_started->handle, SACN_RECEIVER_INVALID);
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
      EXPECT_EQ(sampling_ended->callback, nullptr);
      EXPECT_EQ(sampling_ended->handle, SACN_RECEIVER_INVALID);
      EXPECT_EQ(sampling_ended->context, nullptr);
    }
#else
    SamplingEndedNotification* sampling_ended_buf = get_sampling_ended_buffer(thread, SACN_RECEIVER_MAX_UNIVERSES);
    ASSERT_NE(sampling_ended_buf, nullptr);

    // Test up to the maximum capacity
    for (int i = 0; i < SACN_RECEIVER_MAX_THREADS; ++i)
    {
      auto sampling_ended = &sampling_ended_buf[i];
      EXPECT_EQ(sampling_ended->callback, nullptr);
      EXPECT_EQ(sampling_ended->handle, SACN_RECEIVER_INVALID);
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
  sampling_ended->callback = reinterpret_cast<SacnSamplingPeriodEndedCallback>(kMagicPointerValue);
  sampling_ended->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  sampling_ended = get_sampling_ended_buffer(0, 1);
  ASSERT_NE(sampling_ended, nullptr);

  EXPECT_EQ(sampling_ended->callback, nullptr);
  EXPECT_EQ(sampling_ended->handle, SACN_RECEIVER_INVALID);
  EXPECT_EQ(sampling_ended->context, nullptr);
}

TEST_F(TestMem, ValidInitializedSourceLimitExceeded)
{
  DoForEachThread([](sacn_thread_id_t thread) {
    SourceLimitExceededNotification* source_limit_exceeded = get_source_limit_exceeded(thread);
    ASSERT_NE(source_limit_exceeded, nullptr);

    EXPECT_EQ(source_limit_exceeded->callback, nullptr);
    EXPECT_EQ(source_limit_exceeded->handle, SACN_RECEIVER_INVALID);
    EXPECT_EQ(source_limit_exceeded->context, nullptr);
  });
}

TEST_F(TestMem, SourceLimitExceededIsReZeroedWithEachGet)
{
  SourceLimitExceededNotification* source_limit_exceeded = get_source_limit_exceeded(0);
  ASSERT_NE(source_limit_exceeded, nullptr);

  // Modify some elements
  source_limit_exceeded->handle = 2;
  source_limit_exceeded->callback = reinterpret_cast<SacnSourceLimitExceededCallback>(kMagicPointerValue);
  source_limit_exceeded->context = reinterpret_cast<void*>(kMagicPointerValue);

  // Now get again and make sure they are re-zeroed
  source_limit_exceeded = get_source_limit_exceeded(0);
  ASSERT_NE(source_limit_exceeded, nullptr);

  EXPECT_EQ(source_limit_exceeded->callback, nullptr);
  EXPECT_EQ(source_limit_exceeded->handle, SACN_RECEIVER_INVALID);
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

  etcpal::Uuid last_cid;
  for (size_t i = 0u; i < kNumSources; ++i)
  {
    EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), i);
    last_cid = etcpal::Uuid::V4();
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_remote_source_t>(i), false),
              kEtcPalErrOk);
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources);

  EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_remote_source_t>(kNumSources - 1u), false),
            kEtcPalErrExists);

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources);
}

TEST_F(TestMem, RemoveSacnMergeReceiverSourceWorks)
{
  static constexpr size_t kNumSources = 5u;

  SacnMergeReceiver* merge_receiver = nullptr;
  EXPECT_EQ(add_sacn_merge_receiver(kTestMergeReceiverHandle, &kTestMergeReceiverConfig, &merge_receiver),
            kEtcPalErrOk);

  for (size_t i = 0u; i < kNumSources; ++i)
  {
    EXPECT_EQ(add_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_remote_source_t>(i), false),
              kEtcPalErrOk);
  }

  for (size_t i = 0u; i < kNumSources; ++i)
  {
    EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), kNumSources - i);

    remove_sacn_merge_receiver_source(merge_receiver, static_cast<sacn_remote_source_t>(i));
  }

  EXPECT_EQ(etcpal_rbtree_size(&merge_receiver->sources), 0u);
}
