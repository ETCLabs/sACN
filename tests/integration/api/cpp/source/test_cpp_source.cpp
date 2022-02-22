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
static constexpr uint16_t kTestUniverse2 = 456u;

static etcpal_socket_t next_socket = (etcpal_socket_t)0;

class TestSource : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();

    PopulateFakeNetints();

    etcpal_netint_get_num_interfaces_fake.return_val = fake_netints_.size();
    etcpal_netint_get_interfaces_fake.return_val = fake_netints_.data();

    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
      EXPECT_NE(new_sock, nullptr);
      *new_sock = next_socket++;
      return kEtcPalErrOk;
    };

    etcpal_netint_get_interfaces_by_index_fake.custom_fake = [](unsigned int index, const EtcPalNetintInfo** netint_arr,
                                                                size_t* netint_arr_size) {
      for (auto& fake_netint : fake_netints_)
      {
        if (fake_netint.index == index)
        {
          *netint_arr = &fake_netint;
          *netint_arr_size = 1;
          return kEtcPalErrOk;
        }
      }
      return kEtcPalErrNotFound;
    };

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
  }

  void TearDown() override { Deinit(); }

  void PopulateFakeNetints()
  {
    // Add a fake IPv4-only interface
    EtcPalNetintInfo fake_netint_v4;
    fake_netint_v4.index = 1u;
    fake_netint_v4.addr = etcpal::IpAddr::FromString("10.101.20.30").get();
    fake_netint_v4.mask = etcpal::IpAddr::FromString("255.255.0.0").get();
    fake_netint_v4.mac = etcpal::MacAddr::FromString("00:c0:16:22:22:22").get();
    strcpy(fake_netint_v4.id, "eth_v4_0");
    strcpy(fake_netint_v4.friendly_name, "eth_v4_0");
    fake_netint_v4.is_default = true;
    fake_netints_.push_back(fake_netint_v4);

    // Add a fake IPv6-only interface
    EtcPalNetintInfo fake_netint_v6;
    fake_netint_v6.index = 2u;
    fake_netint_v6.addr = etcpal::IpAddr::FromString("fe80::1234").get();
    fake_netint_v6.mask = etcpal::IpAddr::NetmaskV6(64).get();
    fake_netint_v6.mac = etcpal::MacAddr::FromString("00:c0:16:33:33:33").get();
    strcpy(fake_netint_v6.id, "eth_v6_0");
    strcpy(fake_netint_v6.friendly_name, "eth_v6_0");
    fake_netint_v6.is_default = false;
    fake_netints_.push_back(fake_netint_v6);

    // Add IPv4 test interfaces
    for (auto netint : kTestV4Netints)
    {
      fake_netint_v4.index = netint.iface.index;
      ++fake_netint_v4.addr.addr.v4;
      ++fake_netint_v4.mac.data[ETCPAL_MAC_BYTES - 1];
      ++fake_netint_v4.id[7];
      ++fake_netint_v4.friendly_name[7];
      fake_netint_v4.is_default = false;
      fake_netints_.push_back(fake_netint_v4);
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

std::vector<EtcPalNetintInfo> TestSource::fake_netints_;

TEST_F(TestSource, AddingLotsOfUniversesWorks)
{
  Source source;
  EXPECT_EQ(source.Startup(Source::Settings(etcpal::Uuid::V4(), "Test Source Name")).code(), kEtcPalErrOk);

  for (uint16_t universe = 1u; universe <= 256u; ++universe)
    EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(universe)).code(), kEtcPalErrOk);

  source.Shutdown();
}

TEST_F(TestSource, AddUniverseHandlesTerminationCorrectly)
{
  Source source;
  EXPECT_EQ(source.Startup(Source::Settings(etcpal::Uuid::V4(), "Test Source Name")).code(), kEtcPalErrOk);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrExists);
  source.UpdateLevels(kTestUniverse, nullptr, 0u);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrExists);
  source.RemoveUniverse(kTestUniverse);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
}

TEST_F(TestSource, AddUnicastDestHandlesTerminationCorrectly)
{
  const etcpal::IpAddr kTestAddr = etcpal::IpAddr::FromString("10.101.1.1");

  Source source;
  EXPECT_EQ(source.Startup(Source::Settings(etcpal::Uuid::V4(), "Test Source Name")).code(), kEtcPalErrOk);
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  EXPECT_EQ(source.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrOk);
  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  EXPECT_EQ(source.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrExists);
  source.UpdateLevels(kTestUniverse, nullptr, 0u);
  EXPECT_EQ(source.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrExists);
  source.RemoveUnicastDestination(kTestUniverse, kTestAddr);
  EXPECT_EQ(source.AddUnicastDestination(kTestUniverse, kTestAddr).code(), kEtcPalErrOk);
}

TEST_F(TestSource, UniverseRemovalUsesOldNetintsAsAllowedByPerUniverseReset)
{
  static constexpr int kNumCurrentNetints = 3;

  Source source;
  EXPECT_EQ(source.Startup(Source::Settings(etcpal::Uuid::V4(), "Test Source Name")).code(), kEtcPalErrOk);

  std::deque<SacnMcastInterface> future_sys_netints(kTestV4Netints);
  std::deque<SacnMcastInterface> current_sys_netints;
  for (int i = 0; i < kNumCurrentNetints; ++i)
  {
    // current_sys_netints starts out with the "old" netints which the terminating universe holds on to.
    current_sys_netints.push_back(future_sys_netints.front());

    // future_sys_netints has the "new" netints which the terminating universe never uses.
    future_sys_netints.pop_front();
  }

  ResetNetworking(source, current_sys_netints);

  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse)).code(), kEtcPalErrOk);
  source.UpdateLevels(kTestUniverse, kTestBuffer.data(), kTestBuffer.size());
  source.RemoveUniverse(kTestUniverse);

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
  EXPECT_EQ(source.AddUniverse(Source::UniverseSettings(kTestUniverse2)).code(), kEtcPalErrOk);

  std::vector<Source::UniverseNetintList> universe_netint_lists(
      1, Source::UniverseNetintList(source.handle().value(), kTestUniverse2));

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

      ResetNetworkingPerUniverse(source, current_sys_netints, universe_netint_lists);
    }
  }
}
