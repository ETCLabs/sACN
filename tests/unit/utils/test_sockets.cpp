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
  SubscriptionInfo(const std::vector<etcpal_socket_t>& sockets, uint16_t universe, const EtcPalIpAddr& ip,
                   const std::vector<unsigned int>& netint_indexes)
      : sockets(sockets), universe(universe), ip(ip), netint_indexes(netint_indexes)
  {
  }

  std::vector<etcpal_socket_t> sockets;
  uint16_t universe;
  EtcPalIpAddr ip;
  std::vector<unsigned int> netint_indexes;
};

class TestSockets : public ::testing::Test
{
protected:
  static void ConvertToNetintIds(const std::vector<EtcPalNetintInfo>& in, std::vector<EtcPalMcastNetintId>& out)
  {
    out.reserve(in.size());
    std::transform(in.data(), in.data() + in.size(), std::back_inserter(out), [](const EtcPalNetintInfo& netint) {
      return EtcPalMcastNetintId{netint.addr.type, netint.index};
    });
  }

  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();

    // Add fake interfaces (make sure to add them in order of index)
    if (fake_netint_info_.empty())
    {
      EtcPalNetintInfo fake_netint;

      fake_netint.index = 1;
      fake_netint.addr = etcpal::IpAddr::FromString("10.101.20.30").get();
      fake_netint.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:22:22:22").get();
      strcpy(fake_netint.id, "eth0");
      strcpy(fake_netint.friendly_name, "eth0");
      fake_netint.is_default = true;
      fake_netint_info_.push_back(fake_netint);
      fake_v4_netint_info_.push_back(fake_netint);

      fake_netint.index = 2;
      fake_netint.addr = etcpal::IpAddr::FromString("fe80::1234").get();
      fake_netint.mask = etcpal::IpAddr::NetmaskV6(64).get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:33:33:33").get();
      strcpy(fake_netint.id, "eth1");
      strcpy(fake_netint.friendly_name, "eth1");
      fake_netint.is_default = false;
      fake_netint_info_.push_back(fake_netint);
      fake_v6_netint_info_.push_back(fake_netint);

      fake_netint.index = 3;
      fake_netint.addr = etcpal::IpAddr::FromString("10.101.40.50").get();
      fake_netint.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:12:12:12").get();
      strcpy(fake_netint.id, "eth2");
      strcpy(fake_netint.friendly_name, "eth2");
      fake_netint.is_default = false;
      fake_netint_info_.push_back(fake_netint);
      fake_v4_netint_info_.push_back(fake_netint);

      fake_netint.index = 4;
      fake_netint.addr = etcpal::IpAddr::FromString("fe80::4321").get();
      fake_netint.mask = etcpal::IpAddr::NetmaskV6(64).get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:34:34:34").get();
      strcpy(fake_netint.id, "eth3");
      strcpy(fake_netint.friendly_name, "eth3");
      fake_netint.is_default = false;
      fake_netint_info_.push_back(fake_netint);
      fake_v6_netint_info_.push_back(fake_netint);

      fake_netint.index = 5;
      fake_netint.addr = etcpal::IpAddr::FromString("10.101.60.70").get();
      fake_netint.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:11:11:11").get();
      strcpy(fake_netint.id, "eth4");
      strcpy(fake_netint.friendly_name, "eth4");
      fake_netint.is_default = false;
      fake_netint_info_.push_back(fake_netint);
      fake_v4_netint_info_.push_back(fake_netint);

      fake_netint.index = 6;
      fake_netint.addr = etcpal::IpAddr::FromString("fe80::1122").get();
      fake_netint.mask = etcpal::IpAddr::NetmaskV6(64).get();
      fake_netint.mac = etcpal::MacAddr::FromString("00:c0:16:43:43:43").get();
      strcpy(fake_netint.id, "eth5");
      strcpy(fake_netint.friendly_name, "eth5");
      fake_netint.is_default = false;
      fake_netint_info_.push_back(fake_netint);
      fake_v6_netint_info_.push_back(fake_netint);
    }

    static auto validate_get_interfaces_args = [](EtcPalNetintInfo* netints, size_t* num_netints) {
      if (!num_netints)
        return kEtcPalErrInvalid;
      if ((!netints && (*num_netints > 0)) && (netints && (*num_netints == 0)))
        return kEtcPalErrInvalid;
      return kEtcPalErrOk;
    };

    static auto copy_out_interfaces = [](const EtcPalNetintInfo* copy_src, size_t copy_size, EtcPalNetintInfo* netints,
                                         size_t* num_netints) {
      etcpal_error_t result = kEtcPalErrOk;

      size_t space_available = *num_netints;
      *num_netints = copy_size;

      if (copy_size > space_available)
      {
        result = kEtcPalErrBufSize;
        copy_size = space_available;
      }

      if (netints)
        memcpy(netints, copy_src, copy_size * sizeof(EtcPalNetintInfo));

      return result;
    };

    etcpal_netint_get_interfaces_fake.custom_fake = [](EtcPalNetintInfo* netints, size_t* num_netints) {
      etcpal_error_t result = validate_get_interfaces_args(netints, num_netints);

      if (result == kEtcPalErrOk)
        result = copy_out_interfaces(fake_netint_info_.data(), fake_netint_info_.size(), netints, num_netints);

      return result;
    };

    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
      EXPECT_NE(new_sock, nullptr);
      *new_sock = next_socket++;
      return kEtcPalErrOk;
    };

    ConvertToNetintIds(fake_netint_info_, fake_netint_ids_);
    ConvertToNetintIds(fake_v4_netint_info_, fake_v4_netint_ids_);
    ConvertToNetintIds(fake_v6_netint_info_, fake_v6_netint_ids_);

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

    internal_sys_netints_ = sacn_sockets_get_sys_netints(kReceiver);
    ASSERT_NE(internal_sys_netints_, nullptr);
  }

  void TearDown() override
  {
    sacn_sockets_deinit();
    sacn_receiver_mem_deinit();
  }

  etcpal_error_t AddReceiverSockets(sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t universe,
                                    const std::vector<EtcPalMcastNetintId>& netints,
                                    std::vector<etcpal_socket_t>& sockets)
  {
    etcpal_socket_t socket = ETCPAL_SOCKET_INVALID;
    etcpal_error_t res = kEtcPalErrOk;

#if SACN_RECEIVER_SOCKET_PER_NIC
    for (const auto& netint : netints)
    {
      if (netint.ip_type == ip_type)
      {
        res = sacn_add_receiver_socket(thread_id, ip_type, universe, &netint, 1, &socket);
        if (res == kEtcPalErrOk)
          sockets.push_back(socket);
        else
          break;
      }
    }
#else   // SACN_RECEIVER_SOCKET_PER_NIC
    res = sacn_add_receiver_socket(thread_id, ip_type, universe, netints.data(), netints.size(), &socket);
    if (res == kEtcPalErrOk)
      sockets.push_back(socket);
#endif  // SACN_RECEIVER_SOCKET_PER_NIC

    return res;
  }

  void RemoveReceiverSockets(sacn_thread_id_t thread_id, std::vector<etcpal_socket_t>& sockets, uint16_t universe,
                             const std::vector<EtcPalMcastNetintId>& netints,
                             socket_cleanup_behavior_t cleanup_behavior)
  {
#if SACN_RECEIVER_SOCKET_PER_NIC
    ETCPAL_UNUSED_ARG(netints);

    SacnRecvThreadContext* context = get_recv_thread_context(thread_id);
    for (auto socket : sockets)
    {
      int index = find_socket_ref_by_handle(context, socket);
      ASSERT_TRUE(index >= 0);

      EtcPalMcastNetintId netint;
      netint.ip_type = context->socket_refs[index].socket.ip_type;
      netint.index = context->socket_refs[index].socket.ifindex;

      sacn_remove_receiver_socket(thread_id, &socket, universe, &netint, 1, cleanup_behavior);
    }
#else   // SACN_RECEIVER_SOCKET_PER_NIC
    for (auto socket : sockets)
      sacn_remove_receiver_socket(thread_id, &socket, universe, netints.data(), netints.size(), cleanup_behavior);
#endif  // SACN_RECEIVER_SOCKET_PER_NIC
  }

  SubscriptionInfo QueueSubscribes(sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, size_t iteration)
  {
    uint16_t universe = static_cast<uint16_t>(iteration + 1u);
    std::vector<etcpal_socket_t> sockets;

    EXPECT_EQ(AddReceiverSockets(thread_id, ip_type, universe, fake_netint_ids_, sockets), kEtcPalErrOk)
        << "Test failed on iteration " << iteration << ".";

    EtcPalIpAddr ip;
    sacn_get_mcast_addr(ip_type, universe, &ip);

    return SubscriptionInfo(sockets, universe, ip, (ip_type == kEtcPalIpTypeV4) ? fake_v4_netints_ : fake_v6_netints_);
  }

  void QueueUnsubscribes(sacn_thread_id_t thread_id, const SubscriptionInfo& sub)
  {
    std::vector<etcpal_socket_t> sockets = sub.sockets;
    RemoveReceiverSockets(thread_id, sockets, sub.universe, fake_netint_ids_, kQueueSocketCleanup);
  }

  void VerifyQueue(const SocketGroupReq* queue, const std::vector<SubscriptionInfo>& expected_subs)
  {
    size_t queue_index = 0u;
    for (const SubscriptionInfo& expected_sub : expected_subs)
    {
      size_t socket_index = 0u;
      for (unsigned int expected_netint : expected_sub.netint_indexes)
      {
        EXPECT_EQ(queue[queue_index].socket, expected_sub.sockets[socket_index])
            << "Test failed on queue index " << queue_index << ".";
        EXPECT_EQ(queue[queue_index].group.ifindex, expected_netint)
            << "Test failed on queue index " << queue_index << ".";
        EXPECT_EQ(etcpal_ip_cmp(&queue[queue_index].group.group, &expected_sub.ip), 0)
            << "Test failed on queue index " << queue_index << ".";
        ++queue_index;
#if SACN_RECEIVER_SOCKET_PER_NIC
        ++socket_index;
#endif
      }
    }
  }

  std::vector<SacnMcastInterface> GetFullAppNetintConfig()
  {
    std::vector<SacnMcastInterface> app_netint_config;
    if (app_netint_config.empty())
    {
      for (const auto& sys_netint : fake_netint_info_)
        app_netint_config.push_back({{sys_netint.addr.type, sys_netint.index}, kEtcPalErrOk});
    }
    return app_netint_config;
  }

  enum SamplingStatus
  {
    kCurrentlySampling,
    kNotCurrentlySampling
  };
  void TestSamplingPeriodNetintUpdate(SacnInternalNetintArray* internal_netint_array, SamplingStatus sampling_status,
                                      EtcPalRbTree* sampling_period_netints,
                                      std::vector<SacnMcastInterface>& app_netint_config)
  {
    if (sampling_status == kNotCurrentlySampling)
      etcpal_rbtree_clear_with_cb(sampling_period_netints, sampling_period_netint_tree_dealloc);

    std::vector<SacnMcastInterface> new_netints = app_netint_config;
    auto new_end = std::remove_if(new_netints.begin(), new_netints.end(), [&](const SacnMcastInterface& netint) {
      bool found = false;
      for (size_t i = 0; !found && (i < internal_netint_array->num_netints); ++i)
        found = (netint.iface == internal_netint_array->netints[i]);
      return found;
    });
    new_netints.erase(new_end, new_netints.end());

    std::vector<EtcPalMcastNetintId> removed_sp_netints;
    EtcPalRbIter iter;
    etcpal_rbiter_init(&iter);
    for (SacnSamplingPeriodNetint* sp_netint =
             reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_first(&iter, sampling_period_netints));
         sp_netint; sp_netint = reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_next(&iter)))
    {
      bool removed =
          std::find_if(app_netint_config.begin(), app_netint_config.end(), [&](const SacnMcastInterface& app_netint) {
            return (app_netint.iface == sp_netint->id);
          }) == std::end(app_netint_config);

      if (removed)
        removed_sp_netints.push_back(sp_netint->id);
    }

    size_t original_sp_netints_size = etcpal_rbtree_size(sampling_period_netints);

    SacnNetintConfig c_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
    c_netint_config.netints = app_netint_config.data();
    c_netint_config.num_netints = app_netint_config.size();
    c_netint_config.no_netints = app_netint_config.empty();

    EXPECT_EQ(sacn_initialize_receiver_netints(internal_netint_array, (sampling_status == kCurrentlySampling),
                                               sampling_period_netints, &c_netint_config),
              kEtcPalErrOk);

    EXPECT_EQ(etcpal_rbtree_size(sampling_period_netints),
              original_sp_netints_size + new_netints.size() - removed_sp_netints.size());

    etcpal_rbiter_init(&iter);
    for (SacnSamplingPeriodNetint* sp_netint =
             reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_first(&iter, sampling_period_netints));
         sp_netint; sp_netint = reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_next(&iter)))
    {
      bool found_in_app_config =
          std::find_if(app_netint_config.begin(), app_netint_config.end(), [&](const SacnMcastInterface& app_netint) {
            return (app_netint.iface == sp_netint->id);
          }) != std::end(app_netint_config);
      EXPECT_TRUE(found_in_app_config);

      if (sampling_status == kCurrentlySampling)
      {
        bool is_new = std::find_if(new_netints.begin(), new_netints.end(), [&](const SacnMcastInterface& new_netint) {
                        return (new_netint.iface == sp_netint->id);
                      }) != std::end(new_netints);
        if (is_new)
        {
          EXPECT_TRUE(sp_netint->in_future_sampling_period);
        }
      }
      else
      {
        EXPECT_FALSE(sp_netint->in_future_sampling_period);
      }
    }
  }

  std::vector<SacnMcastInterface> GenerateDuplicateNetints(size_t num_duplicates)
  {
    std::vector<SacnMcastInterface> netints;
    netints.reserve(fake_netint_info_.size() * num_duplicates);
    for (size_t i = 0u; i < num_duplicates; ++i)
    {
      std::transform(fake_netint_info_.data(), fake_netint_info_.data() + fake_netint_info_.size(),
                     std::back_inserter(netints), [](const EtcPalNetintInfo& netint) {
                       return SacnMcastInterface{{netint.addr.type, netint.index}, kEtcPalErrNotImpl};
                     });
    }

    return netints;
  }

  SacnInternalNetintArray InitInternalNetintArray()
  {
    SacnInternalNetintArray internal_netint_array;
#if SACN_DYNAMIC_MEM
    internal_netint_array.netints = nullptr;
    internal_netint_array.netints_capacity = 0u;
#endif
    internal_netint_array.num_netints = 0u;

    return internal_netint_array;
  }

  EtcPalRbTree InitSamplingPeriodNetints()
  {
    EtcPalRbTree sampling_period_netints;
    etcpal_rbtree_init(&sampling_period_netints, sampling_period_netint_compare, sampling_period_netint_node_alloc,
                       sampling_period_netint_node_dealloc);

    return sampling_period_netints;
  }

  void DeinitInternalNetintArray(SacnInternalNetintArray& internal_netint_array)
  {
    CLEAR_BUF(&internal_netint_array, netints);
  }

  void DeinitSamplingPeriodNetints(EtcPalRbTree& sampling_period_netints)
  {
    etcpal_rbtree_clear_with_cb(&sampling_period_netints, sampling_period_netint_tree_dealloc);
  }

  static std::vector<EtcPalNetintInfo> fake_netint_info_;
  static std::vector<EtcPalNetintInfo> fake_v4_netint_info_;
  static std::vector<EtcPalNetintInfo> fake_v6_netint_info_;
  std::vector<EtcPalMcastNetintId> fake_netint_ids_;
  std::vector<EtcPalMcastNetintId> fake_v4_netint_ids_;
  std::vector<EtcPalMcastNetintId> fake_v6_netint_ids_;
  std::vector<unsigned int> fake_v4_netints_;
  std::vector<unsigned int> fake_v6_netints_;
  const SacnSocketsSysNetints* internal_sys_netints_{nullptr};
};

std::vector<EtcPalNetintInfo> TestSockets::fake_netint_info_;
std::vector<EtcPalNetintInfo> TestSockets::fake_v4_netint_info_;
std::vector<EtcPalNetintInfo> TestSockets::fake_v6_netint_info_;

TEST_F(TestSockets, SocketCleanedUpOnBindFailure)
{
  etcpal_bind_fake.return_val = kEtcPalErrAddrNotAvail;

  unsigned int initial_socket_call_count = etcpal_socket_fake.call_count;
  unsigned int initial_close_call_count = etcpal_close_fake.call_count;

  std::vector<etcpal_socket_t> sockets;
  EXPECT_EQ(AddReceiverSockets(0, kEtcPalIpTypeV4, 1, fake_netint_ids_, sockets), kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count - initial_socket_call_count,
            etcpal_close_fake.call_count - initial_close_call_count);
  EXPECT_EQ(AddReceiverSockets(0, kEtcPalIpTypeV6, 1, fake_netint_ids_, sockets), kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count - initial_socket_call_count,
            etcpal_close_fake.call_count - initial_close_call_count);
}

TEST_F(TestSockets, AddReceiverSocketWorks)
{
  static constexpr size_t kNumIterations = 4u;
  static constexpr size_t kNumAddsPerIteration = 2u;
#if SACN_RECEIVER_SOCKET_PER_NIC
  ASSERT_EQ(fake_v4_netints_.size(), fake_v6_netints_.size());
  const size_t kNumSocketsPerAdd = fake_v4_netints_.size();
#else
  const size_t kNumSocketsPerAdd = 1u;
#endif
  const size_t kNumSocketsPerIteration = kNumSocketsPerAdd * kNumAddsPerIteration;
  const size_t kTotalNumSockets = kNumIterations * kNumSocketsPerIteration;

  SacnRecvThreadContext* context = get_recv_thread_context(0);
  ASSERT_NE(context, nullptr);
  ASSERT_NE(context->socket_refs, nullptr);

  uint16_t universe = 1u;
  for (size_t i = 0u; i < kTotalNumSockets; i += kNumSocketsPerIteration)
  {
    for (size_t j = 0u; j < SACN_RECEIVER_MAX_SUBS_PER_SOCKET; ++j)
    {
      EXPECT_EQ(context->num_socket_refs, j ? (i + kNumSocketsPerIteration) : i);

      std::vector<etcpal_socket_t> ipv4_sockets;
      EXPECT_EQ(AddReceiverSockets(0, kEtcPalIpTypeV4, universe, fake_v4_netint_ids_, ipv4_sockets), kEtcPalErrOk);
      ASSERT_EQ(ipv4_sockets.size(), kNumSocketsPerAdd);
      EXPECT_EQ(context->num_socket_refs, j ? (i + kNumSocketsPerIteration) : (i + kNumSocketsPerAdd));
      for (size_t k = 0u; k < kNumSocketsPerAdd; ++k)
      {
        EXPECT_EQ(context->socket_refs[i + k].socket.ip_type, kEtcPalIpTypeV4);
        EXPECT_EQ(context->socket_refs[i + k].refcount, j + 1u);
        EXPECT_EQ(context->socket_refs[i + k].socket.handle, ipv4_sockets[k]);
#if SACN_RECEIVER_SOCKET_PER_NIC
        EXPECT_EQ(context->socket_refs[i + k].socket.ifindex, fake_v4_netints_[k]);
#endif
      }

      std::vector<etcpal_socket_t> ipv6_sockets;
      EXPECT_EQ(AddReceiverSockets(0, kEtcPalIpTypeV6, universe, fake_v6_netint_ids_, ipv6_sockets), kEtcPalErrOk);
      ASSERT_EQ(ipv6_sockets.size(), kNumSocketsPerAdd);
      EXPECT_EQ(context->num_socket_refs, i + (2u * kNumSocketsPerAdd));
      for (size_t k = 0u; k < kNumSocketsPerAdd; ++k)
      {
        EXPECT_EQ(context->socket_refs[i + kNumSocketsPerAdd + k].socket.ip_type, kEtcPalIpTypeV6);
        EXPECT_EQ(context->socket_refs[i + kNumSocketsPerAdd + k].refcount, j + 1u);
        EXPECT_EQ(context->socket_refs[i + kNumSocketsPerAdd + k].socket.handle, ipv6_sockets[k]);
#if SACN_RECEIVER_SOCKET_PER_NIC
        EXPECT_EQ(context->socket_refs[i + kNumSocketsPerAdd + k].socket.ifindex, fake_v6_netints_[k]);
#endif
      }

      ++universe;
    }
  }
}

TEST_F(TestSockets, AddReceiverSocketBindsAfterRemoveUnbinds)
{
  static constexpr sacn_thread_id_t kThreadId = 0u;
  static constexpr uint16_t kUniverse = 1u;
#if SACN_RECEIVER_SOCKET_PER_NIC && !SACN_RECEIVER_LIMIT_BIND
  ASSERT_EQ(fake_v4_netint_ids_.size(), fake_v6_netint_ids_.size());
  const unsigned int kNumBindsPerAdd = static_cast<unsigned int>(fake_v4_netint_ids_.size());
#else
  const unsigned int kNumBindsPerAdd = 1u;
#endif

  std::vector<etcpal_socket_t> sockets;
  unsigned int expected_bind_count = 0u;

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_v4_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  RemoveReceiverSockets(kThreadId, sockets, kUniverse, fake_netint_ids_, kPerformAllSocketCleanupNow);
  sockets.clear();

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_v4_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  // Also consider queued close, which in this case is considered unbinding.
  RemoveReceiverSockets(kThreadId, sockets, kUniverse, fake_netint_ids_, kQueueSocketCleanup);
  sockets.clear();

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_v4_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_v6_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  RemoveReceiverSockets(kThreadId, sockets, kUniverse, fake_netint_ids_, kPerformAllSocketCleanupNow);
  sockets.clear();

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_v6_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  RemoveReceiverSockets(kThreadId, sockets, kUniverse, fake_netint_ids_, kQueueSocketCleanup);
  sockets.clear();

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_v6_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);
}

TEST_F(TestSockets, AddReceiverSocketBindsAfterCreateSocketFails)
{
  static constexpr sacn_thread_id_t kThreadId = 0u;
  static constexpr uint16_t kUniverse = 1u;
#if SACN_RECEIVER_SOCKET_PER_NIC && !SACN_RECEIVER_LIMIT_BIND
  ASSERT_EQ(fake_v4_netint_ids_.size(), fake_v6_netint_ids_.size());
  const unsigned int kNumBindsPerSuccessfulAdd = static_cast<unsigned int>(fake_v4_netint_ids_.size());
#else
  const size_t kNumBindsPerSuccessfulAdd = 1u;
#endif

  std::vector<etcpal_socket_t> sockets;
  unsigned int expected_bind_count = 0u;

  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
    EXPECT_NE(new_sock, nullptr);
    return kEtcPalErrSys;
  };

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_v4_netint_ids_, sockets), kEtcPalErrSys);

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_v6_netint_ids_, sockets), kEtcPalErrSys);

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
    EXPECT_NE(new_sock, nullptr);
    *new_sock = next_socket++;
    return kEtcPalErrOk;
  };
  etcpal_bind_fake.return_val = kEtcPalErrSys;

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_v4_netint_ids_, sockets), kEtcPalErrSys);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_v6_netint_ids_, sockets), kEtcPalErrSys);

  ++expected_bind_count;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  etcpal_bind_fake.return_val = kEtcPalErrOk;

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, kUniverse, fake_v4_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerSuccessfulAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, kUniverse, fake_v6_netint_ids_, sockets), kEtcPalErrOk);

  expected_bind_count += kNumBindsPerSuccessfulAdd;
  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);
}

TEST_F(TestSockets, AddAndRemoveReceiverSocketBindWhenNeeded)
{
  static constexpr sacn_thread_id_t kThreadId = 0u;
  static constexpr uint16_t kStartUniverse = 1u;
  static constexpr int kNumIterations = 4;

  ASSERT_EQ(fake_v4_netint_ids_.size(), fake_v6_netint_ids_.size());

  std::vector<etcpal_socket_t> sockets[SACN_RECEIVER_MAX_SUBS_PER_SOCKET * kNumIterations * 2];
  uint16_t universe = kStartUniverse;
  unsigned int expected_bind_count = 0u;

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  for (int i = 0; i < (SACN_RECEIVER_MAX_SUBS_PER_SOCKET * kNumIterations); ++i)
  {
    EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV4, universe, fake_v4_netint_ids_, sockets[i * 2]),
              kEtcPalErrOk);
    EXPECT_EQ(AddReceiverSockets(kThreadId, kEtcPalIpTypeV6, universe, fake_v6_netint_ids_, sockets[(i * 2) + 1]),
              kEtcPalErrOk);

    ++universe;
  }

#if SACN_RECEIVER_LIMIT_BIND
  expected_bind_count += 2;
#elif SACN_RECEIVER_SOCKET_PER_NIC
  ASSERT_EQ(fake_v4_netint_ids_.size(), fake_v6_netint_ids_.size());
  expected_bind_count += (kNumIterations * 2) * static_cast<unsigned int>(fake_v4_netint_ids_.size());
#else
  expected_bind_count += (kNumIterations * 2);
#endif

  EXPECT_EQ(etcpal_bind_fake.call_count, expected_bind_count);

  universe = kStartUniverse;
  for (int i = 0; i < kNumIterations; ++i)
  {
    for (int j = 0; j < SACN_RECEIVER_MAX_SUBS_PER_SOCKET; ++j)
    {
      int ipv4_sockets_index = ((SACN_RECEIVER_MAX_SUBS_PER_SOCKET * i) + j) * 2;
      int ipv6_sockets_index = (((SACN_RECEIVER_MAX_SUBS_PER_SOCKET * i) + j) * 2) + 1;
      RemoveReceiverSockets(kThreadId, sockets[ipv4_sockets_index], universe, fake_v4_netint_ids_,
                            kPerformAllSocketCleanupNow);
      RemoveReceiverSockets(kThreadId, sockets[ipv6_sockets_index], universe, fake_v6_netint_ids_,
                            kPerformAllSocketCleanupNow);

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

  SacnNetintConfig app_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  app_netint_config.netints = app_netints.data();
  app_netint_config.num_netints = app_netints.size();

  size_t num_valid_netints = 0u;
  ASSERT_EQ(sacn_validate_netint_config(&app_netint_config, sys_netints.data(), sys_netints.size(), &num_valid_netints),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_initialize_internal_netints(&internal_netint_array, &app_netint_config, num_valid_netints,
                                             sys_netints.data(), sys_netints.size()),
            kEtcPalErrOk);

  for (size_t i = 0u; i < app_netints.size(); ++i)
    EXPECT_EQ(app_netints[i].status, expected_statuses[i]);

  EXPECT_EQ(internal_netint_array.num_netints, expected_internal_netints.size());

  for (size_t i = 0u; i < internal_netint_array.num_netints; ++i)
  {
    EXPECT_EQ(internal_netint_array.netints[i].index, expected_internal_netints[i].index);
    EXPECT_EQ(internal_netint_array.netints[i].ip_type, expected_internal_netints[i].ip_type);
  }

  CLEAR_BUF(&internal_netint_array, netints);

  // Test "no interfaces" as well
  app_netint_config.netints = nullptr;
  app_netint_config.num_netints = 0u;
  app_netint_config.no_netints = true;

  ASSERT_EQ(sacn_validate_netint_config(&app_netint_config, sys_netints.data(), sys_netints.size(), &num_valid_netints),
            kEtcPalErrOk);
  EXPECT_EQ(sacn_initialize_internal_netints(&internal_netint_array, &app_netint_config, num_valid_netints,
                                             sys_netints.data(), sys_netints.size()),
            kEtcPalErrOk);

  EXPECT_EQ(num_valid_netints, 0u);
  EXPECT_EQ(internal_netint_array.num_netints, 0u);

  CLEAR_BUF(&internal_netint_array, netints);
}

TEST_F(TestSockets, SamplingPeriodNetintsUpdateCorrectly)
{
  SacnInternalNetintArray internal_netint_array = InitInternalNetintArray();
  EtcPalRbTree sampling_period_netints = InitSamplingPeriodNetints();

  std::vector<SacnMcastInterface> full_config = GetFullAppNetintConfig();
  std::vector<SacnMcastInterface> config_1 = {full_config.begin(), full_config.end() - (full_config.size() / 2)};
  std::vector<SacnMcastInterface> config_2 = full_config;
  std::vector<SacnMcastInterface> config_3 = {full_config.begin() + (full_config.size() / 2), full_config.end()};
  std::vector<SacnMcastInterface> empty_config = {};  // TestSamplingPeriodNetintUpdate treats empty as "no interfaces"

  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, empty_config);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_1);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_2);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_3);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_2);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_1);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_3);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, config_1);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kNotCurrentlySampling, &sampling_period_netints, empty_config);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, empty_config);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_1);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_2);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_3);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_2);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_1);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_3);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, config_1);
  TestSamplingPeriodNetintUpdate(&internal_netint_array, kCurrentlySampling, &sampling_period_netints, empty_config);

  DeinitInternalNetintArray(internal_netint_array);
  DeinitSamplingPeriodNetints(sampling_period_netints);
}

TEST_F(TestSockets, AddAllNetintsToSamplingPeriodWorks)
{
  std::vector<SacnMcastInterface> sys_netints = {
      {{kEtcPalIpTypeV4, 1u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 2u}, kEtcPalErrOk},
      {{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 4u}, kEtcPalErrOk},
      {{kEtcPalIpTypeV4, 5u}, kEtcPalErrOk}, {{kEtcPalIpTypeV6, 6u}, kEtcPalErrOk}};

  SacnInternalNetintArray internal_netint_array = InitInternalNetintArray();

  SacnNetintConfig app_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;

  size_t num_valid_netints = 0u;
  ASSERT_EQ(sacn_validate_netint_config(&app_netint_config, sys_netints.data(), sys_netints.size(), &num_valid_netints),
            kEtcPalErrOk);
  ASSERT_EQ(sacn_initialize_internal_netints(&internal_netint_array, &app_netint_config, num_valid_netints,
                                             sys_netints.data(), sys_netints.size()),
            kEtcPalErrOk);

  EtcPalRbTree sampling_period_netints = InitSamplingPeriodNetints();
  EXPECT_EQ(add_sacn_sampling_period_netint(&sampling_period_netints, &sys_netints[0].iface, false), kEtcPalErrOk);
  EXPECT_EQ(add_sacn_sampling_period_netint(&sampling_period_netints, &sys_netints[1].iface, true), kEtcPalErrOk);

  EXPECT_EQ(sacn_add_all_netints_to_sampling_period(&internal_netint_array, &sampling_period_netints), kEtcPalErrOk);

  EXPECT_EQ(etcpal_rbtree_size(&sampling_period_netints), sys_netints.size());

  int sys_netints_index = 0;
  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  for (SacnSamplingPeriodNetint* sp_netint =
           reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_first(&iter, &sampling_period_netints));
       sp_netint; sp_netint = reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_next(&iter)))
  {
    auto new_end = std::remove_if(sys_netints.begin(), sys_netints.end(),
                                  [&](const SacnMcastInterface& netint) { return netint.iface == sp_netint->id; });
    EXPECT_NE(new_end, sys_netints.end());  // Verify each sp_netint lines up with exactly one sys_netint
    EXPECT_FALSE(sp_netint->in_future_sampling_period);

    sys_netints.erase(new_end, sys_netints.end());
    ++sys_netints_index;
  }

  DeinitInternalNetintArray(internal_netint_array);
  DeinitSamplingPeriodNetints(sampling_period_netints);
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

  etcpal_error_t tmp_err = kEtcPalErrOk;
  EXPECT_EQ(sacn_send_multicast(kTestUniverseId, kSacnIpV4AndIpV6, send_buf, &fake_netint_ids_[0]), kEtcPalErrOk);
  EXPECT_EQ(sacn_send_unicast(kSacnIpV4AndIpV6, send_buf, &kTestAddr, &tmp_err), kEtcPalErrOk);

  EXPECT_EQ(etcpal_sendto_fake.call_count, 2u);
}

// Init has already been called with nullptr. Verify that it has initialized all sys netints.
TEST_F(TestSockets, InitStartsOnAllNetints)
{
  ASSERT_EQ(internal_sys_netints_->num_sys_netints, fake_netint_info_.size());
  for (size_t i = 0u; i < internal_sys_netints_->num_sys_netints; ++i)
  {
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.index, fake_netint_info_[i].index)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.ip_type, fake_netint_info_[i].addr.type)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].status, kEtcPalErrOk) << "Test failed on iteration " << i << ".";
  }
}

// Now test reset with custom sys netints (just use the receiver variant)
// (this also verifies init since it's the same underlying function)
TEST_F(TestSockets, ResetHandlesCustomNetints)
{
  std::vector<SacnMcastInterface> sys_netints;
  sys_netints.reserve(fake_netint_info_.size());
  std::transform(fake_netint_info_.data(), fake_netint_info_.data() + fake_netint_info_.size(),
                 std::back_inserter(sys_netints), [](const EtcPalNetintInfo& netint) {
                   return SacnMcastInterface{{netint.addr.type, netint.index}, kEtcPalErrNotImpl};
                 });

  // Add some extra nonexistant netints
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV6, 1234}, kEtcPalErrNotImpl});
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV4, 5678}, kEtcPalErrNotImpl});
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV6, 8765}, kEtcPalErrNotImpl});
  sys_netints.push_back(SacnMcastInterface{{kEtcPalIpTypeV4, 4321}, kEtcPalErrNotImpl});

  for (size_t num_sys_netints = sys_netints.size(); num_sys_netints >= 1u; --num_sys_netints)
  {
    SacnNetintConfig sys_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
    sys_netint_config.netints = sys_netints.data();
    sys_netint_config.num_netints = num_sys_netints;

    EXPECT_EQ(sacn_sockets_reset_receiver(&sys_netint_config), kEtcPalErrOk);

    size_t num_valid_netints =
        (num_sys_netints > fake_netint_info_.size()) ? fake_netint_info_.size() : num_sys_netints;
    size_t num_invalid_netints =
        (num_sys_netints > fake_netint_info_.size()) ? (num_sys_netints - fake_netint_info_.size()) : 0u;

    ASSERT_EQ(internal_sys_netints_->num_sys_netints, num_valid_netints);
    for (size_t i = 0u; i < num_valid_netints; ++i)
    {
      EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.index, sys_netints[i].iface.index)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
      EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.ip_type, sys_netints[i].iface.ip_type)
          << "Test failed on iteration " << i << " when testing " << num_sys_netints << " netints.";
      EXPECT_EQ(internal_sys_netints_->sys_netints[i].status, kEtcPalErrOk)
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
}

// Test the "no interfaces" case
TEST_F(TestSockets, ResetHandlesNoNetints)
{
  SacnNetintConfig sys_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  sys_netint_config.no_netints = true;

  EXPECT_EQ(sacn_sockets_reset_receiver(&sys_netint_config), kEtcPalErrOk);

  EXPECT_EQ(internal_sys_netints_->num_sys_netints, 0u);
}

// Test duplicate interfaces
TEST_F(TestSockets, ResetHandlesDuplicateNetints)
{
  static constexpr size_t kNumDuplicates = 3u;
  std::vector<SacnMcastInterface> sys_netints = GenerateDuplicateNetints(kNumDuplicates);

  SacnNetintConfig sys_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  sys_netint_config.netints = sys_netints.data();
  sys_netint_config.num_netints = sys_netints.size();

  EXPECT_EQ(sacn_sockets_reset_receiver(&sys_netint_config), kEtcPalErrOk);

  ASSERT_EQ(internal_sys_netints_->num_sys_netints, fake_netint_info_.size());
  for (size_t i = 0u; i < internal_sys_netints_->num_sys_netints; ++i)
  {
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.index, fake_netint_info_[i].index)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.ip_type, fake_netint_info_[i].addr.type)
        << "Test failed on iteration " << i << ".";
  }

  for (size_t i = 0u; i < sys_netints.size(); ++i)
    EXPECT_EQ(sys_netints[i].status, kEtcPalErrOk) << "Test failed on iteration " << i << ".";
}

// Now return to the nullptr (all sys netints) case
TEST_F(TestSockets, ResetHandlesAllNetints)
{
  EXPECT_EQ(sacn_sockets_reset_receiver(nullptr), kEtcPalErrOk);
  ASSERT_EQ(internal_sys_netints_->num_sys_netints, fake_netint_info_.size());
  for (size_t i = 0u; i < internal_sys_netints_->num_sys_netints; ++i)
  {
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.index, fake_netint_info_[i].index)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].iface.ip_type, fake_netint_info_[i].addr.type)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_sys_netints_->sys_netints[i].status, kEtcPalErrOk) << "Test failed on iteration " << i << ".";
  }
}

TEST_F(TestSockets, InitNetintsHandlesDuplicates)
{
  static constexpr size_t kNumDuplicates = 3u;
  std::vector<SacnMcastInterface> app_netints = GenerateDuplicateNetints(kNumDuplicates);

  SacnNetintConfig c_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  c_netint_config.netints = app_netints.data();
  c_netint_config.num_netints = app_netints.size();

  SacnInternalNetintArray internal_netint_array = InitInternalNetintArray();
  EtcPalRbTree sampling_period_netints = InitSamplingPeriodNetints();

  EXPECT_EQ(sacn_initialize_receiver_netints(&internal_netint_array, false, &sampling_period_netints, &c_netint_config),
            kEtcPalErrOk);

  for (size_t i = 0u; i < app_netints.size(); ++i)
    EXPECT_EQ(app_netints[i].status, kEtcPalErrOk) << "Test failed on iteration " << i << ".";

  ASSERT_EQ(internal_netint_array.num_netints, fake_netint_info_.size());
  for (size_t i = 0u; i < fake_netint_info_.size(); ++i)
  {
    EXPECT_EQ(internal_netint_array.netints[i].index, fake_netint_info_[i].index)
        << "Test failed on iteration " << i << ".";
    EXPECT_EQ(internal_netint_array.netints[i].ip_type, fake_netint_info_[i].addr.type)
        << "Test failed on iteration " << i << ".";
  }

  EXPECT_EQ(etcpal_rbtree_size(&sampling_period_netints), fake_netint_info_.size());
  EtcPalRbIter iter;
  etcpal_rbiter_init(&iter);
  for (SacnSamplingPeriodNetint* sp_netint =
           reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_first(&iter, &sampling_period_netints));
       sp_netint; sp_netint = reinterpret_cast<SacnSamplingPeriodNetint*>(etcpal_rbiter_next(&iter)))
  {
    bool found_in_fake_netints =
        std::find_if(fake_netint_info_.begin(), fake_netint_info_.end(), [&](const EtcPalNetintInfo& fake_netint) {
          return (fake_netint.index == sp_netint->id.index) && (fake_netint.addr.type == sp_netint->id.ip_type);
        }) != std::end(fake_netint_info_);
    EXPECT_TRUE(found_in_fake_netints);
  }

  DeinitInternalNetintArray(internal_netint_array);
  DeinitSamplingPeriodNetints(sampling_period_netints);
}
