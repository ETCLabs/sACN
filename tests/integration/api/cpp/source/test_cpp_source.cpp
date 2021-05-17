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

#include <limits>
#include "etcpal/cpp/uuid.h"
#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "sacn/cpp/common.h"
#include "sacn/cpp/source.h"
#include "sacn/private/opts.h"
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

static etcpal_socket_t next_socket = (etcpal_socket_t)0;

class TestSource : public ::testing::Test
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

    ASSERT_EQ(sacn::Init().code(), kEtcPalErrOk);
  }

  void TearDown() override { sacn::Deinit(); }

  std::vector<EtcPalNetintInfo> fake_netints_;
};

TEST_F(TestSource, AddingLotsOfUniversesWorks)
{
  sacn::Source source;
  EXPECT_EQ(source.Startup(sacn::Source::Settings(etcpal::Uuid::V4(), "Test Source Name")).code(), kEtcPalErrOk);

  for (uint16_t universe = 1u; universe <= 256u; ++universe)
    EXPECT_EQ(source.AddUniverse(sacn::Source::UniverseSettings(universe)).code(), kEtcPalErrOk);

  source.Shutdown();
}
