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

#include "sacn/private/sockets.h"

#include <vector>
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "etcpal/cpp/inet.h"
#include "etcpal/acn_rlp.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn_mock/private/common.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestSockets TestSocketsDynamic
#else
#define TestSockets TestSocketsStatic
#endif

#ifdef _MSC_VER
// disable strcpy() warnings on MSVC
#pragma warning(disable : 4996)
#endif

static etcpal_socket_t next_socket = (etcpal_socket_t)0;

struct SubscriptionInfo
{
  SubscriptionInfo(etcpal_socket_t sock, uint16_t universe, const EtcPalIpAddr& ip,
                   const std::vector<unsigned int>& netint_indexes)
      : sock(sock), universe(universe), ip(ip), netint_indexes(netint_indexes)
  {
  }

  etcpal_socket_t sock;
  uint16_t universe;
  EtcPalIpAddr ip;
  std::vector<unsigned int> netint_indexes;
};

class TestSockets : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();

    // Add fake interfaces (make sure to add them in order of index)
    if (fake_netints_.empty())
    {
      EtcPalNetintInfo fake_netint;

      fake_netint.index = 1;
      fake_netint.addr = etcpal::IpAddr::FromString("10.101.20.30").get();
      fake_netint.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:22:22:22").get();
      strcpy(fake_netint.id, "eth0");
      strcpy(fake_netint.friendly_name, "eth0");
      fake_netint.is_default = true;
      fake_netints_.push_back(fake_netint);

      fake_netint.index = 2;
      fake_netint.addr = etcpal::IpAddr::FromString("fe80::1234").get();
      fake_netint.mask = etcpal::IpAddr::NetmaskV6(64).get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:33:33:33").get();
      strcpy(fake_netint.id, "eth1");
      strcpy(fake_netint.friendly_name, "eth1");
      fake_netint.is_default = false;
      fake_netints_.push_back(fake_netint);

      fake_netint.index = 3;
      fake_netint.addr = etcpal::IpAddr::FromString("10.101.40.50").get();
      fake_netint.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:12:12:12").get();
      strcpy(fake_netint.id, "eth2");
      strcpy(fake_netint.friendly_name, "eth2");
      fake_netint.is_default = false;
      fake_netints_.push_back(fake_netint);

      fake_netint.index = 4;
      fake_netint.addr = etcpal::IpAddr::FromString("fe80::4321").get();
      fake_netint.mask = etcpal::IpAddr::NetmaskV6(64).get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:34:34:34").get();
      strcpy(fake_netint.id, "eth3");
      strcpy(fake_netint.friendly_name, "eth3");
      fake_netint.is_default = false;
      fake_netints_.push_back(fake_netint);

      fake_netint.index = 5;
      fake_netint.addr = etcpal::IpAddr::FromString("10.101.60.70").get();
      fake_netint.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:11:11:11").get();
      strcpy(fake_netint.id, "eth4");
      strcpy(fake_netint.friendly_name, "eth4");
      fake_netint.is_default = false;
      fake_netints_.push_back(fake_netint);
    }

    etcpal_netint_get_num_interfaces_fake.return_val = fake_netints_.size();
    etcpal_netint_get_interfaces_fake.return_val = fake_netints_.data();

    etcpal_netint_get_interfaces_by_index_fake.custom_fake = [](unsigned int index, const EtcPalNetintInfo** netint_arr,
                                                                size_t* netint_arr_size) {
      etcpal_error_t result = (netint_arr && netint_arr_size) ? kEtcPalErrOk : kEtcPalErrInvalid;

      if (result == kEtcPalErrOk)
      {
        EtcPalNetintInfo* info = fake_netints_.data();
        while ((info < (fake_netints_.data() + fake_netints_.size())) && (info->index != index))
          ++info;

        if (info >= (fake_netints_.data() + fake_netints_.size()))
        {
          result = kEtcPalErrNotFound;
        }
        else
        {
          *netint_arr = info;

          size_t size = 0u;
          while ((info < (fake_netints_.data() + fake_netints_.size())) && (info->index == index))
            ++info, ++size;

          *netint_arr_size = size;
        }
      }

      return result;
    };

    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
      EXPECT_NE(new_sock, nullptr);
      *new_sock = next_socket++;
      return kEtcPalErrOk;
    };

    fake_netint_ids_.reserve(fake_netints_.size());
    std::transform(fake_netints_.data(), fake_netints_.data() + fake_netints_.size(),
                   std::back_inserter(fake_netint_ids_), [](const EtcPalNetintInfo& netint) {
                     return EtcPalMcastNetintId{netint.addr.type, netint.index};
                   });

    // Split the netints according to their IP type.
    for (const EtcPalMcastNetintId& fake_netint_id : fake_netint_ids_)
    {
      if (fake_netint_id.ip_type == kEtcPalIpTypeV4)
        fake_v4_netints_.push_back(fake_netint_id.index);
      else if (fake_netint_id.ip_type == kEtcPalIpTypeV6)
        fake_v6_netints_.push_back(fake_netint_id.index);
    }

    ASSERT_GT(fake_v4_netints_.size(), 0u);
    ASSERT_GT(fake_v6_netints_.size(), 0u);

    ASSERT_EQ(sacn_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_sockets_init(nullptr), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_sockets_deinit();
    sacn_receiver_mem_deinit();
  }

  SubscriptionInfo QueueSubscribes(sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, size_t iteration)
  {
    uint16_t universe = static_cast<uint16_t>(iteration + 1u);
    etcpal_socket_t sock{};

    EXPECT_EQ(
        sacn_add_receiver_socket(thread_id, ip_type, universe, fake_netint_ids_.data(), fake_netint_ids_.size(), &sock),
        kEtcPalErrOk)
        << "Test failed on iteration " << iteration << ".";

    EtcPalIpAddr ip;
    sacn_get_mcast_addr(ip_type, universe, &ip);

    return SubscriptionInfo(sock, universe, ip, (ip_type == kEtcPalIpTypeV4) ? fake_v4_netints_ : fake_v6_netints_);
  }

  void QueueUnsubscribes(sacn_thread_id_t thread_id, const SubscriptionInfo& sub)
  {
    etcpal_socket_t sock = sub.sock;
    sacn_remove_receiver_socket(thread_id, &sock, sub.universe, fake_netint_ids_.data(), fake_netint_ids_.size(),
                                kQueueSocketCleanup);
  }

  void VerifyQueue(const SocketGroupReq* queue, std::vector<SubscriptionInfo> expected_subs)
  {
    size_t queue_index = 0u;
    for (const SubscriptionInfo& expected_sub : expected_subs)
    {
      for (unsigned int expected_netint : expected_sub.netint_indexes)
      {
        EXPECT_EQ(queue[queue_index].socket, expected_sub.sock) << "Test failed on queue index " << queue_index << ".";
        EXPECT_EQ(queue[queue_index].group.ifindex, expected_netint)
            << "Test failed on queue index " << queue_index << ".";
        EXPECT_EQ(etcpal_ip_cmp(&queue[queue_index].group.group, &expected_sub.ip), 0)
            << "Test failed on queue index " << queue_index << ".";
        ++queue_index;
      }
    }
  }

  static std::vector<EtcPalNetintInfo> fake_netints_;
  std::vector<EtcPalMcastNetintId> fake_netint_ids_;
  std::vector<unsigned int> fake_v4_netints_;
  std::vector<unsigned int> fake_v6_netints_;
};

std::vector<EtcPalNetintInfo> TestSockets::fake_netints_;

TEST_F(TestSockets, SocketCleanedUpOnBindFailure)
{
  etcpal_bind_fake.return_val = kEtcPalErrAddrNotAvail;

  unsigned int initial_socket_call_count = etcpal_socket_fake.call_count;
  unsigned int initial_close_call_count = etcpal_close_fake.call_count;

  etcpal_socket_t sock;
  EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV4, 1, fake_netint_ids_.data(), fake_netint_ids_.size(), &sock),
            kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count - initial_socket_call_count,
            etcpal_close_fake.call_count - initial_close_call_count);
  EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV6, 1, fake_netint_ids_.data(), fake_netint_ids_.size(), &sock),
            kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count - initial_socket_call_count,
            etcpal_close_fake.call_count - initial_close_call_count);
}

TEST_F(TestSockets, AddReceiverSocketWorks)
{
  SacnRecvThreadContext* context = get_recv_thread_context(0);
  ASSERT_NE(context, nullptr);
  ASSERT_NE(context->socket_refs, nullptr);

  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  uint16_t universe = 1u;
  for (size_t i = 0u; i < 8u; i += 2u)
  {
    for (size_t j = 0u; j < SACN_RECEIVER_MAX_SUBS_PER_SOCKET; ++j)
    {
      EXPECT_EQ(context->num_socket_refs, j ? (i + 2u) : i);

      EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV4, universe, fake_netint_ids_.data(), fake_netint_ids_.size(),
                                         &sock),
                kEtcPalErrOk);
      EXPECT_EQ(context->num_socket_refs, j ? (i + 2u) : (i + 1u));
      EXPECT_EQ(context->socket_refs[i].socket.ip_type, kEtcPalIpTypeV4);
      EXPECT_EQ(context->socket_refs[i].refcount, j + 1u);
      EXPECT_EQ(context->socket_refs[i].socket.handle, sock);

      EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV6, universe, fake_netint_ids_.data(), fake_netint_ids_.size(),
                                         &sock),
                kEtcPalErrOk);
      EXPECT_EQ(context->num_socket_refs, i + 2u);
      EXPECT_EQ(context->socket_refs[i + 1u].socket.ip_type, kEtcPalIpTypeV6);
      EXPECT_EQ(context->socket_refs[i + 1u].refcount, j + 1u);
      EXPECT_EQ(context->socket_refs[i + 1u].socket.handle, sock);

      ++universe;
    }
  }
}

TEST_F(TestSockets, AddReceiverSocketBindsAfterRemoveUnbinds)
{
  static constexpr sacn_thread_id_t kThreadId = 0u;
  static constexpr uint16_t kUniverse = 1u;

  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  unsigned int expected_bind_count = 0u;

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  sacn_remove_receiver_socket(kThreadId, &sock, kUniverse, fake_netint_ids_.data(), fake_netint_ids_.size(),
                              kPerformAllSocketCleanupNow);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  // Also consider queued close, which in this case is considered unbinding.
  sacn_remove_receiver_socket(kThreadId, &sock, kUniverse, fake_netint_ids_.data(), fake_netint_ids_.size(),
                              kQueueSocketCleanup);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  sacn_remove_receiver_socket(kThreadId, &sock, kUniverse, fake_netint_ids_.data(), fake_netint_ids_.size(),
                              kPerformAllSocketCleanupNow);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  sacn_remove_receiver_socket(kThreadId, &sock, kUniverse, fake_netint_ids_.data(), fake_netint_ids_.size(),
                              kQueueSocketCleanup);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);
}

TEST_F(TestSockets, AddReceiverSocketBindsAfterCreateSocketFails)
{
  static constexpr sacn_thread_id_t kThreadId = 0u;
  static constexpr uint16_t kUniverse = 1u;

  etcpal_socket_t sock = ETCPAL_SOCKET_INVALID;
  unsigned int expected_bind_count = 0u;

  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
    EXPECT_NE(new_sock, nullptr);
    return kEtcPalErrSys;
  };

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrSys);

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrSys);

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
    EXPECT_NE(new_sock, nullptr);
    *new_sock = next_socket++;
    return kEtcPalErrOk;
  };
  etcpal_bind_fake.return_val = kEtcPalErrSys;

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrSys);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrSys);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  etcpal_bind_fake.return_val = kEtcPalErrOk;

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_netint_ids_.data(),
                                     fake_netint_ids_.size(), &sock),
            kEtcPalErrOk);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);
}

TEST_F(TestSockets, AddAndRemoveReceiverSocketBindWhenNeeded)
{
  static constexpr sacn_thread_id_t kThreadId = 0u;
  static constexpr uint16_t kStartUniverse = 1u;
  static constexpr int kNumIterations = 4;

  etcpal_socket_t sock[SACN_RECEIVER_MAX_SUBS_PER_SOCKET * kNumIterations * 2];
  uint16_t universe = kStartUniverse;
  unsigned int expected_bind_count = 0u;

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  for (int i = 0; i < (SACN_RECEIVER_MAX_SUBS_PER_SOCKET * kNumIterations); ++i)
  {
    EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV4, universe, fake_netint_ids_.data(),
                                       fake_netint_ids_.size(), &sock[i * 2]),
              kEtcPalErrOk);
    EXPECT_EQ(sacn_add_receiver_socket(kThreadId, kEtcPalIpTypeV6, universe, fake_netint_ids_.data(),
                                       fake_netint_ids_.size(), &sock[(i * 2) + 1]),
              kEtcPalErrOk);

    ++universe;
  }

#if SACN_RECEIVER_LIMIT_BIND
  expected_bind_count += 2;
#else
  expected_bind_count += (kNumIterations * 2);
#endif

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  universe = kStartUniverse;
  for (int i = 0; i < kNumIterations; ++i)
  {
    for (int j = 0; j < SACN_RECEIVER_MAX_SUBS_PER_SOCKET; ++j)
    {
      int ipv4_socket_index = ((SACN_RECEIVER_MAX_SUBS_PER_SOCKET * i) + j) * 2;
      int ipv6_socket_index = (((SACN_RECEIVER_MAX_SUBS_PER_SOCKET * i) + j) * 2) + 1;
      sacn_remove_receiver_socket(kThreadId, &sock[ipv4_socket_index], universe, fake_netint_ids_.data(),
                                  fake_netint_ids_.size(), kPerformAllSocketCleanupNow);
      sacn_remove_receiver_socket(kThreadId, &sock[ipv6_socket_index], universe, fake_netint_ids_.data(),
                                  fake_netint_ids_.size(), kPerformAllSocketCleanupNow);

      ++universe;
    }

#if SACN_RECEIVER_LIMIT_BIND
    if (i < (kNumIterations - 1))
      expected_bind_count += 2;
#endif
    EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);
  }
}

TEST_F(TestSockets, SubscribeAndUnsubscribeQueueCorrectly)
{
  static constexpr size_t kNumSubscriptions = 30u;
  static constexpr sacn_thread_id_t kThreadId = 0u;

  SacnRecvThreadContext* context = get_recv_thread_context(kThreadId);
  ASSERT_NE(context, nullptr);

  // Store subscription info to compare against the subscribe/unsubscribe queues later.
  std::vector<SubscriptionInfo> v4_v6_subs;
  std::vector<SubscriptionInfo> v4_subs;
  std::vector<SubscriptionInfo> v6_subs;

  // Queue subscriptions on IPv4 and IPv6.
  for (size_t i = 0u; i < kNumSubscriptions; ++i)
  {
    EXPECT_EQ(context->num_subscribes, i * fake_netint_ids_.size()) << "Test failed on iteration " << i << ".";
    EXPECT_EQ(context->num_unsubscribes, 0u) << "Test failed on iteration " << i << ".";

    SubscriptionInfo v4_sub = QueueSubscribes(kThreadId, kEtcPalIpTypeV4, i);
    SubscriptionInfo v6_sub = QueueSubscribes(kThreadId, kEtcPalIpTypeV6, i);
    v4_v6_subs.push_back(v4_sub);
    v4_v6_subs.push_back(v6_sub);
    v4_subs.push_back(v4_sub);
    v6_subs.push_back(v6_sub);
  }

  // Check that the queues contain the expected subscription operations.
  ASSERT_EQ(context->num_subscribes, kNumSubscriptions * fake_netint_ids_.size());
  EXPECT_EQ(context->num_unsubscribes, 0u);
  VerifyQueue(context->subscribes, v4_v6_subs);

  // Now unsubscribe only the IPv4 subscriptions.
  // This should remove them from the subscribe queue without touching the unsubscribe queue.
  for (const SubscriptionInfo& v4_sub : v4_subs)
    QueueUnsubscribes(kThreadId, v4_sub);

  // Check that the queues are correct.
  ASSERT_EQ(context->num_subscribes, v6_subs.size() * fake_v6_netints_.size());
  EXPECT_EQ(context->num_unsubscribes, 0u);
  VerifyQueue(context->subscribes, v6_subs);

  // Now empty the subscribe queue as if it was processed.
  context->num_subscribes = 0u;

  // Now unsubscribe the IPv6 subscriptions.
  // This should actually add to the unsubscribe queue this time, since the subscribe queue is empty.
  for (const SubscriptionInfo& v6_sub : v6_subs)
    QueueUnsubscribes(kThreadId, v6_sub);

  // Check that the queues are correct.
  EXPECT_EQ(context->num_subscribes, 0u);
  ASSERT_EQ(context->num_unsubscribes, v6_subs.size() * fake_v6_netints_.size());
  VerifyQueue(context->unsubscribes, v6_subs);

  // Now subscribe IPv6 again. Brand new sockets are created because all socket refs were destroyed.
  for (size_t i = 0u; i < v6_subs.size(); ++i)
    QueueSubscribes(kThreadId, kEtcPalIpTypeV6, i);

  // Because the sockets are new, the old sockets should be in unsubscribes, while the new ones should be in subscribes.
  EXPECT_EQ(context->num_subscribes, v6_subs.size() * fake_v6_netints_.size());
  EXPECT_EQ(context->num_unsubscribes, v6_subs.size() * fake_v6_netints_.size());
}

TEST_F(TestSockets, InitializeInternalNetintsWorks)
{
  std::vector<SacnMcastInterface> sys_netints = {
      {{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk},         {{kEtcPalIpTypeV6, 2u}, kEtcPalErrNetwork},
      {{kEtcPalIpTypeV4, 3u}, kEtcPalErrConnClosed}, {{kEtcPalIpTypeV6, 4u}, kEtcPalErrSys},
      {{kEtcPalIpTypeV4, 5u}, kEtcPalErrOk},         {{kEtcPalIpTypeV6, 6u}, kEtcPalErrOk}};
  std::vector<SacnMcastInterface> app_netints = {
      {{kEtcPalIpTypeV4, 0u}, kEtcPalErrOk}, {{kEtcPalIpTypeInvalid, 1u}, kEtcPalErrOk},
      {{kEtcPalIpTypeV6, 1u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 2u}, kEtcPalErrOk},
      {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 4u}, kEtcPalErrOk},
      {{kEtcPalIpTypeV4, 5u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 6u}, kEtcPalErrOk},
      {{kEtcPalIpTypeV4, 7u}, kEtcPalErrOk}};

  std::vector<etcpal_error_t> expected_statuses = {kEtcPalErrInvalid, kEtcPalErrInvalid,    kEtcPalErrNotFound,
                                                   kEtcPalErrNetwork, kEtcPalErrConnClosed, kEtcPalErrSys,
                                                   kEtcPalErrOk,      kEtcPalErrOk,         kEtcPalErrNotFound};
  std::vector<EtcPalMcastNetintId> expected_internal_netints = {{kEtcPalIpTypeV4, 5u}, {kEtcPalIpTypeV6, 6u}};

  ASSERT_EQ(app_netints.size(), expected_statuses.size());

  SacnInternalNetintArray internal_netint_array;
#if SACN_DYNAMIC_MEM
  internal_netint_array.netints = nullptr;
  internal_netint_array.netints_capacity = 0u;
#endif
  internal_netint_array.num_netints = 0u;

  SacnNetintConfig app_netint_config = {app_netints.data(), app_netints.size()};
  sacn_initialize_internal_netints(&internal_netint_array, &app_netint_config, sys_netints.data(), sys_netints.size());

  for (size_t i = 0u; i < app_netints.size(); ++i)
    EXPECT_EQ(app_netints[i].status, expected_statuses[i]);

  EXPECT_EQ(internal_netint_array.num_netints, expected_internal_netints.size());

  for (size_t i = 0u; i < internal_netint_array.num_netints; ++i)
  {
    EXPECT_EQ(internal_netint_array.netints[i].index, expected_internal_netints[i].index);
    EXPECT_EQ(internal_netint_array.netints[i].ip_type, expected_internal_netints[i].ip_type);
  }

  CLEAR_BUF(&internal_netint_array, netints);
}

TEST_F(TestSockets, SendTransmitsMinimumLength)
{
  constexpr uint16_t kTestUniverseId = 123u;
  const EtcPalIpAddr kTestAddr = etcpal::IpAddr::FromString("10.101.40.50").get();
  static constexpr uint16_t kTestLength = 123u;

  uint8_t send_buf[SACN_MTU] = {0};
  ACN_PDU_PACK_NORMAL_LEN(&send_buf[ACN_UDP_PREAMBLE_SIZE], kTestLength);

  etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void*, size_t length, int, const EtcPalSockAddr*) {
    EXPECT_EQ(length, ACN_UDP_PREAMBLE_SIZE + kTestLength);
    return 0;
  };

  EXPECT_EQ(etcpal_sendto_fake.call_count, 0u);

  sacn_send_multicast(kTestUniverseId, kSacnIpV4AndIpV6, send_buf, &fake_netint_ids_[0]);
  sacn_send_unicast(kSacnIpV4AndIpV6, send_buf, &kTestAddr);

  EXPECT_EQ(etcpal_sendto_fake.call_count, 3u);
}

TEST_F(TestSockets, InitAndResetHandleCustomSysNetints)
{
  const SacnSocketsSysNetints* internal_sys_netints = sacn_sockets_get_sys_netints(kReceiver);
  ASSERT_NE(internal_sys_netints, nullptr);

  // This starts with init having already been called with nullptr (using all sys netints). Verify that.
  ASSERT_EQ(internal_sys_netints->num_sys_netints, fake_netints_.size());
  for (size_t i = 0u; i < internal_sys_netints->num_sys_netints; ++i)
  {
    EXPECT_EQ(internal_sys_netints->sys_netints[i].iface.index, fake_netints_[i].index)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints->sys_netints[i].iface.ip_type, fake_netints_[i].addr.type)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints->sys_netints[i].status, kEtcPalErrOk) << "Test failed on iteration " << i << ".";
  }

  // Now test reset with custom sys netints (just use the receiver variant)
  // (this also verifies init since it's the same underlying function)
  std::vector<SacnMcastInterface> sys_netints;
  sys_netints.reserve(fake_netints_.size());
  std::transform(fake_netints_.data(), fake_netints_.data() + fake_netints_.size(), std::back_inserter(sys_netints),
                 [](const EtcPalNetintInfo& netint) {
                   return SacnMcastInterface{{netint.addr.type, netint.index}, kEtcPalErrNotImpl};
                 });

  // Add some extra nonexistant netints
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV6, 1234}, kEtcPalErrNotImpl});
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV4, 5678}, kEtcPalErrNotImpl});
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV6, 8765}, kEtcPalErrNotImpl});
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV4, 4321}, kEtcPalErrNotImpl});

  for (size_t num_sys_netints = sys_netints.size(); num_sys_netints >= 1u; --num_sys_netints)
  {
    SacnNetintConfig sys_netint_config = {sys_netints.data(), num_sys_netints};
    EXPECT_EQ(sacn_sockets_reset_receiver(&sys_netint_config), kEtcPalErrOk);

    size_t num_valid_netints = (num_sys_netints > fake_netints_.size()) ? fake_netints_.size() : num_sys_netints;
    size_t num_invalid_netints =
        (num_sys_netints > fake_netints_.size()) ? (num_sys_netints - fake_netints_.size()) : 0u;

    ASSERT_EQ(internal_sys_netints->num_sys_netints, num_valid_netints);
    for (size_t i = 0u; i < num_valid_netints; ++i)
    {
      EXPECT_EQ(internal_sys_netints->sys_netints[i].iface.index, sys_netints[i].iface.index)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
      EXPECT_EQ(internal_sys_netints->sys_netints[i].iface.ip_type, sys_netints[i].iface.ip_type)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
      EXPECT_EQ(internal_sys_netints->sys_netints[i].status, kEtcPalErrOk)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
      EXPECT_EQ(sys_netints[i].status, kEtcPalErrOk)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
    }

    for (size_t i = num_valid_netints; i < (num_valid_netints + num_invalid_netints); ++i)
    {
      EXPECT_EQ(sys_netints[i].status, kEtcPalErrNotFound)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
    }
  }

  // Now return to the nullptr (all sys netints) case
  EXPECT_EQ(sacn_sockets_reset_receiver(nullptr), kEtcPalErrOk);
  ASSERT_EQ(internal_sys_netints->num_sys_netints, fake_netints_.size());
  for (size_t i = 0u; i < internal_sys_netints->num_sys_netints; ++i)
  {
    EXPECT_EQ(internal_sys_netints->sys_netints[i].iface.index, fake_netints_[i].index)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints->sys_netints[i].iface.ip_type, fake_netints_[i].addr.type)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints->sys_netints[i].status, kEtcPalErrOk) << "Test failed on iteration " << i << ".";
  }
}
