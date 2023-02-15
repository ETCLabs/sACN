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

#include "sacn/cpp/source.h"

#include <limits>
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "sacn/private/opts.h"
#include "sacn/private/sockets.h"
#include "sacn/private/source_state.h"
#include "gtest/gtest.h"

#if SACN_DYNAMIC_MEM
#define TestIpv4Ipv6 TestCppIpv4Ipv6Dynamic
#else
#define TestIpv4Ipv6 TestCppIpv4Ipv6Static
#endif

#ifdef _MSC_VER
// disable strcpy() warnings on MSVC
#pragma warning(disable : 4996)
#endif

using namespace sacn;

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};

static constexpr uint16_t kTestUniverse = 123u;

typedef struct FakeNetworkInfo
{
  unsigned int index;
  etcpal_iptype_t type;
  char addr[20];
  char mask_v4[20];
  unsigned int mask_v6;
  char mac[20];
  char name[ETCPAL_NETINTINFO_ID_LEN];
  bool is_default;
  bool found_dest_addr;
} FakeNetworkInfo;

static std::vector<FakeNetworkInfo> fake_networks_info = {
    {1u, kEtcPalIpTypeV4, "10.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:22", "eth_v4_0", true, false},
    {2u, kEtcPalIpTypeV6, "fe80::1234", "", 64u, "00:c0:16:33:33:33", "eth_v6_0", false, false},
};

static etcpal_socket_t next_socket = (etcpal_socket_t)0;

class TestIpv4Ipv6 : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();

    PopulateFakeNetints();

    ResetFoundInfo();

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

    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
      EXPECT_NE(new_sock, nullptr);
      *new_sock = next_socket++;
      return kEtcPalErrOk;
    };

    etcpal_netint_get_interfaces_fake.custom_fake = [](EtcPalNetintInfo* netints, size_t* num_netints) {
      auto result = validate_get_interfaces_args(netints, num_netints);
      if (result != kEtcPalErrOk)
        return result;

      return copy_out_interfaces(fake_netints_.data(), fake_netints_.size(), netints, num_netints);
    };

    etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void*, size_t, int, const EtcPalSockAddr* dest_addr) {
      for (int i = 0; i < fake_networks_info.size(); i++)
      {
        EtcPalIpAddr ip;
        sacn_get_mcast_addr(fake_networks_info[i].type, kTestUniverse, &ip);
        if (etcpal_ip_cmp(&dest_addr->ip, &ip) == 0)
        {
          fake_networks_info[i].found_dest_addr = true;
          break;
        }
      }

      return 0;
    };

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
  }

  void TearDown() override { Deinit(); }

  void PopulateFakeNetints()
  {
    for (auto fake_network_info : fake_networks_info)
    {
      EtcPalNetintInfo fake_netint;
      fake_netint.index = fake_network_info.index;
      fake_netint.addr = etcpal::IpAddr::FromString(fake_network_info.addr).get();
      if (fake_network_info.type == kEtcPalIpTypeV4)
      {
        fake_netint.mask = etcpal::IpAddr::FromString(fake_network_info.mask_v4).get();
      }
      else
      {
        fake_netint.mask = etcpal::IpAddr::NetmaskV6(fake_network_info.mask_v6).get();
      }
      fake_netint.mac = etcpal::MacAddr::FromString(fake_network_info.mac).get();
      strcpy(fake_netint.id, fake_network_info.name);
      strcpy(fake_netint.friendly_name, fake_network_info.name);
      fake_netint.is_default = fake_network_info.is_default;
      fake_netints_.push_back(fake_netint);
    }
  }

  void ResetFoundInfo()
  {
    for (int i = 0; i < fake_networks_info.size(); i++)
    {
      fake_networks_info[i].found_dest_addr = false;
    }
  }

  void RunThreadCycle() { take_lock_and_process_sources(kProcessThreadedSources); }

  void ResetNetworking(Source& source, const std::deque<SacnMcastInterface>& sys_netints)
  {
    std::vector<SacnMcastInterface> vect(sys_netints.begin(), sys_netints.end());
    EXPECT_TRUE(source.ResetNetworking(vect).IsOk());
  }

  void ResetNetworkingPerUniverse(Source& source, const std::deque<SacnMcastInterface>& sys_netints,
                                  std::vector<Source::UniverseNetintList>& netint_lists)
  {
    std::vector<SacnMcastInterface> vect(sys_netints.begin(), sys_netints.end());
    EXPECT_TRUE(source.ResetNetworking(vect, netint_lists).IsOk());
  }

  static std::vector<EtcPalNetintInfo> fake_netints_;
};

std::vector<EtcPalNetintInfo> TestIpv4Ipv6::fake_netints_;

TEST_F(TestIpv4Ipv6, IPv4Works)
{
  Source::Settings settings(etcpal::Uuid::V4(), "Test IPv4 Source Name");
  settings.ip_supported = kSacnIpV4Only;
  Source source;
  EXPECT_EQ(source.Startup(settings).code(), kEtcPalErrOk);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());

  for (int i = 0; i < 4; ++i)
    RunThreadCycle();

  for (auto fake_network_info : fake_networks_info)
  {
    if (fake_network_info.type == kEtcPalIpTypeV4)
      EXPECT_TRUE(fake_network_info.found_dest_addr);
    else
      EXPECT_FALSE(fake_network_info.found_dest_addr);
  }

  source.Shutdown();
}

TEST_F(TestIpv4Ipv6, IPv6Works)
{
  Source::Settings settings(etcpal::Uuid::V4(), "Test IPv6 Source Name");
  settings.ip_supported = kSacnIpV6Only;
  Source source;
  EXPECT_EQ(source.Startup(settings).code(), kEtcPalErrOk);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());

  for (int i = 0; i < 4; ++i)
    RunThreadCycle();

  for (auto fake_network_info : fake_networks_info)
  {
    if (fake_network_info.type == kEtcPalIpTypeV4)
      EXPECT_FALSE(fake_network_info.found_dest_addr);
    else
      EXPECT_TRUE(fake_network_info.found_dest_addr);
  }

  source.Shutdown();
}

TEST_F(TestIpv4Ipv6, IPv4AndIPv6WorkTogether)
{
  Source::Settings settings(etcpal::Uuid::V4(), "Test IPv4 / IPv6 Source Name");
  settings.ip_supported = kSacnIpV4AndIpV6;
  Source source;
  EXPECT_EQ(source.Startup(settings).code(), kEtcPalErrOk);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());

  for (int i = 0; i < 4; ++i)
    RunThreadCycle();

  for (auto fake_network_info : fake_networks_info)
  {
    EXPECT_TRUE(fake_network_info.found_dest_addr);
  }

  source.Shutdown();
}
