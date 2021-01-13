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

#include "sacn/private/sockets.h"

#include <vector>
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "etcpal/cpp/inet.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
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

    ASSERT_EQ(sacn_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_sockets_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_sockets_deinit();
    sacn_mem_deinit();
  }

  std::vector<EtcPalNetintInfo> fake_netints_;
};

TEST_F(TestSockets, GoodNetintConfigValidated)
{
  std::vector<SacnMcastInterface> netints;
  netints.push_back({{kEtcPalIpTypeV4, 1}, kEtcPalErrOk});
  netints.push_back({{kEtcPalIpTypeV6, 30}, kEtcPalErrOk});
  netints.push_back({{kEtcPalIpTypeV4, 1000}, kEtcPalErrOk});
  EXPECT_EQ(sacn_validate_netint_config(netints.data(), netints.size(), nullptr), kEtcPalErrOk);
}

TEST_F(TestSockets, EmptyNetintConfigValidated)
{
  EXPECT_EQ(sacn_validate_netint_config(NULL, 0, nullptr), kEtcPalErrOk);
}

// TODO: Tests for initialize_internal_netints?

TEST_F(TestSockets, SocketCleanedUpOnBindFailure)
{
  etcpal_bind_fake.return_val = kEtcPalErrAddrNotAvail;

  etcpal_socket_t sock;
  EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV4, 1, NULL, 0, &sock), kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
  EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV6, 1, NULL, 0, &sock), kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
}

TEST_F(TestSockets, SocketCleanedUpOnSubscribeFailure)
{
  etcpal_setsockopt_fake.return_val = kEtcPalErrAddrNotAvail;

  etcpal_socket_t sock;
  EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV4, 1, NULL, 0, &sock), kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
  EXPECT_EQ(sacn_add_receiver_socket(0, kEtcPalIpTypeV6, 1, NULL, 0, &sock), kEtcPalErrAddrNotAvail);
  EXPECT_EQ(etcpal_socket_fake.call_count, etcpal_close_fake.call_count);
}
