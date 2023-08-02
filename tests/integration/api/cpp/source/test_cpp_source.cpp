/******************************************************************************
 * Copyright 2023 ETC Inc.
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
#define TestSource TestCppSourceDynamic
#else
#define TestSource TestCppSourceStatic
#endif

#ifdef _MSC_VER
// disable strcpy() warnings on MSVC
#pragma warning(disable : 4996)
#endif

using namespace sacn;

static const std::vector<uint8_t> kTestBuffer = {
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu,
};

static const std::deque<SacnMcastInterface> kTestV4Netints = {{{kEtcPalIpTypeV4, 3u}, kEtcPalErrOk},
                                                              {{kEtcPalIpTypeV4, 4u}, kEtcPalErrOk},
                                                              {{kEtcPalIpTypeV4, 5u}, kEtcPalErrOk},
                                                              {{kEtcPalIpTypeV4, 6u}, kEtcPalErrOk},
                                                              {{kEtcPalIpTypeV4, 7u}, kEtcPalErrOk}};

static constexpr uint16_t kTestUniverse = 123u;
static const std::string kTestUniverseIPv4Multicast = "239.255.0.123";  // can't use 'constexpr' with std::string
static const std::string kTestUniverseIPv6Multicast = "ff18::8300:7b";
static constexpr uint16_t kTestUniverse2 = 456u;

static etcpal_socket_t next_socket = (etcpal_socket_t)0;

typedef struct FakeNetworkInfo
{
  unsigned int index;
  etcpal_iptype_t type;
  std::string addr;
  std::string mask_v4;
  unsigned int mask_v6;
  std::string mac;
  std::string name;
  bool is_default;
} FakeNetworkInfo;

static const std::vector<FakeNetworkInfo> kFakeNetworksInfo = {
    {1u, kEtcPalIpTypeV4, "10.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:22", "eth_v4_0", true},
    {2u, kEtcPalIpTypeV6, "fe80::1234", "", 64u, "00:c0:16:33:33:33", "eth_v6_0", false},
    {3u, kEtcPalIpTypeV4, "20.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:23", "eth_v4_1", false},
    {4u, kEtcPalIpTypeV4, "30.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:24", "eth_v4_2", false},
    {5u, kEtcPalIpTypeV4, "40.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:25", "eth_v4_3", false},
    {6u, kEtcPalIpTypeV4, "50.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:26", "eth_v4_4", false},
    {7u, kEtcPalIpTypeV4, "60.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:27", "eth_v4_5", false},
};

typedef struct UnicastInfo
{
  std::string addr_string;
  bool found;
} UnicastInfo;

class TestSourceBase : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();

    PopulateFakeNetints();

    ResetMulticastSentInfo();

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
  }

  void PopulateFakeNetints()
  {
    EtcPalNetintInfo fake_netint;
    for (auto fake_network_info : kFakeNetworksInfo)
    {
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
      strcpy(fake_netint.id, fake_network_info.name.c_str());
      strcpy(fake_netint.friendly_name, fake_network_info.name.c_str());
      fake_netint.is_default = fake_network_info.is_default;
      fake_netints_.push_back(fake_netint);
    }
  }

  void ResetMulticastSentInfo()
  {
    ipv4_multicast_packet_sent_ = false;
    ipv6_multicast_packet_sent_ = false;
  }

  void Start()
  {
    EXPECT_EQ(source_.Startup(settings_).code(), kEtcPalErrOk);
  }

  void StartAndAddUniverse()
  {
    EXPECT_EQ(source_.Startup(settings_).code(), kEtcPalErrOk);
    EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
    source_.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  }

  void RunThreadCycle()
  {
    take_lock_and_process_sources(kProcessThreadedSources, kSacnSourceTickModeProcessLevelsOnly);
    take_lock_and_process_sources(kProcessThreadedSources, kSacnSourceTickModeProcessPapOnly);
  }

  static bool IsIPv4Multicast(const etcpal::SockAddr& sock_addr)
  {
    return (sock_addr.ip().ToString() == kTestUniverseIPv4Multicast);
  }

  static bool IsIPv6Multicast(const etcpal::SockAddr& sock_addr)
  {
    return (sock_addr.ip().ToString() == kTestUniverseIPv6Multicast);
  }

  void TearDown() override
  {
    source_.Shutdown();
    Deinit();
  }

  static std::vector<EtcPalNetintInfo> fake_netints_;
  static std::vector<UnicastInfo> fake_unicasts_info_;
  static Source source_;
  static Source::Settings settings_;
  static bool ipv4_multicast_packet_sent_;
  static bool ipv6_multicast_packet_sent_;
};

std::vector<EtcPalNetintInfo> TestSourceBase::fake_netints_;
std::vector<UnicastInfo> TestSourceBase::fake_unicasts_info_ = {
    {"10.101.20.1", false},
    {"10.101.20.2", false},
};
Source TestSourceBase::source_;
Source::Settings TestSourceBase::settings_(etcpal::Uuid::V4(), "Test Source");
bool TestSourceBase::ipv4_multicast_packet_sent_;
bool TestSourceBase::ipv6_multicast_packet_sent_;

/*===========================================================================*/

class TestSource : public TestSourceBase
{
protected:
  void SetUp() override
  {
    TestSourceBase::SetUp();

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
  }

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
};

TEST_F(TestSource, AddingLotsOfUniversesWorks)
{
  Start();

  for (uint16_t universe = 1u; universe <= 256u; ++universe)
    EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(universe)).code(), kEtcPalErrOk);

  source_.Shutdown();
}

TEST_F(TestSource, AddUniverseHandlesTerminationCorrectly)
{
  StartAndAddUniverse();
  EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrExists);
  source_.UpdateLevels(kTestUniverse, nullptr, 0u);
  EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrExists);
  source_.RemoveUniverse(kTestUniverse);
  EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
}

TEST_F(TestSource, AddUnicastDestHandlesTerminationCorrectly)
{
  const etcpal::IpAddr kTestAddr = etcpal::IpAddr::FromString("10.101.1.1");

  Start();
  EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);

  EXPECT_EQ(source_.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrOk);
  source_.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  EXPECT_EQ(source_.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrExists);
  source_.UpdateLevels(kTestUniverse, nullptr, 0u);
  EXPECT_EQ(source_.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrExists);
  source_.RemoveUnicastDestination(kTestUniverse, kTestAddr);
  EXPECT_EQ(source_.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrOk);
}

TEST_F(TestSource, UniverseRemovalUsesOldNetintsAsAllowedByPerUniverseReset)
{
  static constexpr int kNumCurrentNetints = 3;

  Start();

  std::deque<SacnMcastInterface> future_sys_netints(kTestV4Netints);
  std::deque<SacnMcastInterface> current_sys_netints;
  for (int i = 0; i < kNumCurrentNetints; ++i)
  {
    // current_sys_netints starts out with the "old" netints which the terminating universe holds on to.
    current_sys_netints.push_back(future_sys_netints.front());

    // future_sys_netints has the "new" netints which the terminating universe never uses.
    future_sys_netints.pop_front();
  }

  ResetNetworking(source_, current_sys_netints);

  EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  source_.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  source_.RemoveUniverse(kTestUniverse);

  // Track number of terminations on multicast.
  static int num_terminations_sent = 0;
  etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void*, size_t, int, const EtcPalSockAddr* dest_addr) {
    EtcPalIpAddr ip;
    sacn_get_mcast_addr(kEtcPalIpTypeV4, kTestUniverse, &ip);

    if (etcpal_ip_cmp(&dest_addr->ip, &ip) == 0)
      ++num_terminations_sent;

    return 0;
  };

  // Add another universe so per-universe network reset can still run.
  EXPECT_EQ(source_.AddUniverse(Source::UniverseSettings(kTestUniverse2)).code(), kEtcPalErrOk);

  std::vector<Source::UniverseNetintList> universe_netint_lists(
      1, Source::UniverseNetintList(source_.handle().value(), kTestUniverse2));

  for (int i = 0; i < 3; ++i)
  {
    int prev_termination_count = num_terminations_sent;
    RunThreadCycle();

    // Old netints being used decreases each time.
    EXPECT_EQ(num_terminations_sent - prev_termination_count, kNumCurrentNetints - i) << "i = " << i;

    if (!future_sys_netints.empty())
    {
      // current_sys_netints drops an old netint and adds a new one, decreasing the old netints that can transmit.
      current_sys_netints.pop_front();
      current_sys_netints.push_back(future_sys_netints.front());
      future_sys_netints.pop_front();

      ResetNetworkingPerUniverse(source_, current_sys_netints, universe_netint_lists);
    }
  }
}

/*===========================================================================*/

class TestSourceIpv4Ipv6 : public TestSourceBase
{
protected:
  void SetUp() override
  {
    TestSourceBase::SetUp();

    etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void*, size_t, int, const EtcPalSockAddr* dest_addr) {
      if (IsIPv4Multicast(etcpal::SockAddr(*dest_addr)))
      {
        ipv4_multicast_packet_sent_ = true;
      }
      else if (IsIPv6Multicast(etcpal::SockAddr(*dest_addr)))
      {
        ipv6_multicast_packet_sent_ = true;
      }

      return 0;
    };

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
  }
  
  void StartAndRunSource(const sacn_ip_support_t ip_supported)
  {
    settings_.ip_supported = ip_supported;
    StartAndAddUniverse();
    for (int i = 0; i < 4; ++i)
      RunThreadCycle();
  }
};

TEST_F(TestSourceIpv4Ipv6, IPv4Works)
{
  StartAndRunSource(kSacnIpV4Only);
  EXPECT_TRUE(ipv4_multicast_packet_sent_);
  EXPECT_FALSE(ipv6_multicast_packet_sent_);
}

TEST_F(TestSourceIpv4Ipv6, IPv6Works)
{
  StartAndRunSource(kSacnIpV6Only);
  EXPECT_FALSE(ipv4_multicast_packet_sent_);
  EXPECT_TRUE(ipv6_multicast_packet_sent_);
}

TEST_F(TestSourceIpv4Ipv6, IPv4AndIPv6WorkTogether)
{
  StartAndRunSource(kSacnIpV4AndIpV6);
  EXPECT_TRUE(ipv4_multicast_packet_sent_);
  EXPECT_TRUE(ipv6_multicast_packet_sent_);
}

/*===========================================================================*/

class TestSourceUnicast : public TestSourceBase
{
protected:
  void SetUp() override
  {
    TestSourceBase::SetUp();

    ResetUnicastSentInfo();

    etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void*, size_t, int, const EtcPalSockAddr* dest_addr) {
      etcpal::SockAddr dest_sock_addr(*dest_addr);
      if (IsIPv4Multicast(dest_sock_addr))
      {
        ipv4_multicast_packet_sent_ = true;
      }
      else if (IsIPv6Multicast(dest_sock_addr))
      {
        ipv6_multicast_packet_sent_ = true;
      }
      else
      {
        for (auto& fake_unicast_info : fake_unicasts_info_)
        {
          if (fake_unicast_info.addr_string == dest_sock_addr.ip().ToString())
          {
            fake_unicast_info.found = true;
          }
        }
      }

      return 0;
    };

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
  }

  void ResetUnicastSentInfo()
  {
    for (auto& fake_unicast_info : fake_unicasts_info_)
    {
      fake_unicast_info.found = false;
    }
  }

  void StartAndRunSource(bool add_unicast)
  {
    StartAndAddUniverse();
    if (add_unicast)
    {
      for (auto fake_unicast_info : fake_unicasts_info_)
      {
        etcpal::IpAddr dest_addr = etcpal::IpAddr::FromString(fake_unicast_info.addr_string);
        source_.AddUnicastDestination(kTestUniverse, dest_addr);
      }
    }
    for (int i = 0; i < 4; ++i)
      RunThreadCycle();
  }
};

TEST_F(TestSourceUnicast, MulticastOnly)
{
  StartAndRunSource(false);
  EXPECT_TRUE(ipv4_multicast_packet_sent_);
  EXPECT_TRUE(ipv6_multicast_packet_sent_);
  for (auto fake_unicast_info : fake_unicasts_info_)
  {
    EXPECT_FALSE(fake_unicast_info.found);
  }
}

TEST_F(TestSourceUnicast, MulticastAndUnicast)
{
  StartAndRunSource(true);
  EXPECT_TRUE(ipv4_multicast_packet_sent_);
  EXPECT_TRUE(ipv6_multicast_packet_sent_);
  for (auto fake_unicast_info : fake_unicasts_info_)
  {
    EXPECT_TRUE(fake_unicast_info.found);
  }
}

/*===========================================================================*/

static constexpr uint8_t kDmxCidOffset = 22u; /* DMX packet CID offset
                                                 preamble:     2 bytes
                                                 postamble:    2 bytes
                                                 "ASC-E1.17": 12 bytes
                                                 flags & len:  2 bytes
                                                 protocol:     4 bytes
                                                 cid:         16 bytes
                                              */

class TestSourceCid : public TestSourceBase
{
protected:
  void SetUp() override
  {
    TestSourceBase::SetUp();

    ResetFoundInfo();

    etcpal_sendto_fake.custom_fake = [](etcpal_socket_t, const void* message, size_t, int,
                                        const EtcPalSockAddr*) {
      etcpal::Uuid dest_cid(&((uint8_t*)message)[kDmxCidOffset]);
      if (settings_.cid != dest_cid)
      {
        cid_found_always_ = false;
      }

      return 0;
    };

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
  }

  void ResetFoundInfo()
  {
    cid_found_always_ = true;
  }

  void StartAndRunSource()
  {
    StartAndAddUniverse();
    for (auto fake_unicast_info : fake_unicasts_info_)
    {
      etcpal::IpAddr dest_addr = etcpal::IpAddr::FromString(fake_unicast_info.addr_string);
      source_.AddUnicastDestination(kTestUniverse, dest_addr);
    }
    for (int i = 0; i < 4; ++i)
      RunThreadCycle();
  }

  static bool cid_found_always_;
};

bool TestSourceCid::cid_found_always_;

TEST_F(TestSourceCid, SourceCID)
{
  StartAndRunSource();
  EXPECT_TRUE(cid_found_always_);
}
