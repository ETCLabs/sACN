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

#include "sacn/private/sockets.h"

#include <vector>
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "etcpal/cpp/inet.h"
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

class TestSockets : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();

    // Add a fake IPv4-only interface
    EtcPalNetintInfo fake_netint_v4;
    fake_netint_v4.index = 1;
    fake_netint_v4.addr = etcpal::IpAddr::FromString("10.101.20.30").get();
    fake_netint_v4.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
    fake_netint_v4.mac = etcpal::MacAddr::FromString("00:c0:16:22:22:22").get();
    strcpy(fake_netint_v4.id, "eth0");
    strcpy(fake_netint_v4.friendly_name, "eth0");
    fake_netint_v4.is_default = true;
    fake_netints_.push_back(fake_netint_v4);

    // Add a fake IPv6-only interface
    EtcPalNetintInfo fake_netint_v6;
    fake_netint_v6.index = 2;
    fake_netint_v6.addr = etcpal::IpAddr::FromString("fe80::1234").get();
    fake_netint_v6.mask = etcpal::IpAddr::NetmaskV6(64).get();
    fake_netint_v6.mac = etcpal::MacAddr::FromString("00:c0:16:33:33:33").get();
    strcpy(fake_netint_v6.id, "eth1");
    strcpy(fake_netint_v6.friendly_name, "eth1");
    fake_netint_v6.is_default = false;
    fake_netints_.push_back(fake_netint_v6);

    etcpal_netint_get_num_interfaces_fake.return_val = fake_netints_.size();
    etcpal_netint_get_interfaces_fake.return_val = fake_netints_.data();
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

    ASSERT_EQ(sacn_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_sockets_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_sockets_deinit();
    sacn_receiver_mem_deinit();
  }

  std::vector<EtcPalNetintInfo> fake_netints_;
  std::vector<EtcPalMcastNetintId> fake_netint_ids_;
};

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

TEST_F(TestSockets, SocketCleanedUpOnSubscribeFailure)
{
  etcpal_setsockopt_fake.return_val = kEtcPalErrAddrNotAvail;

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
      EXPECT_EQ(context->socket_refs[i].ip_type, kEtcPalIpTypeV4);
      EXPECT_EQ(context->socket_refs[i].refcount, j + 1u);
      EXPECT_EQ(context->socket_refs[i].sock, sock);

      EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV6, universe, fake_netint_ids_.data(), fake_netint_ids_.size(),
                                         &sock),
                kEtcPalErrOk);
      EXPECT_EQ(context->num_socket_refs, i + 2u);
      EXPECT_EQ(context->socket_refs[i + 1u].ip_type, kEtcPalIpTypeV6);
      EXPECT_EQ(context->socket_refs[i + 1u].refcount, j + 1u);
      EXPECT_EQ(context->socket_refs[i + 1u].sock, sock);

      ++universe;
    }
  }
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

  sacn_initialize_internal_netints(&internal_netint_array, app_netints.data(), app_netints.size(), sys_netints.data(),
                                   sys_netints.size());

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
