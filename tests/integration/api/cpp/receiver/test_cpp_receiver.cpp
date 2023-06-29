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

#include "sacn/cpp/receiver.h"
#include "sacn/cpp/merge_receiver.h"

#include "etcpal_mock/common.h"
#include "etcpal_mock/netint.h"
#include "etcpal_mock/socket.h"
#include "etcpal_mock/timer.h"
#include "sacn_mock/private/receiver_state.h"
#include "sacn/private/mem/receiver/recv_thread_context.h"
#include "sacn/private/pdu.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

#if SACN_DYNAMIC_MEM
#define TestReceiverBase TestCppReceiverBaseDynamic
#define TestReceiver TestCppReceiverDynamic
#define TestMergeReceiver TestCppMergeReceiverDynamic
#else
#define TestReceiverBase TestCppReceiverBaseStatic
#define TestReceiver TestCppReceiverStatic
#define TestMergeReceiver TestCppMergeReceiverStatic
#endif

#ifdef _MSC_VER
// disable strcpy() warnings on MSVC
#pragma warning(disable : 4996)
#endif

using namespace sacn;

static constexpr uint16_t kTestUniverse = 1u;
static constexpr size_t kCidOffset = 22u;
static constexpr size_t kOptionsOffset = 112u;
static constexpr size_t kSlotsOffset = 126u;

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
  bool got_universe_data;
} FakeNetworkInfo;

static std::vector<FakeNetworkInfo> fake_networks_info = {
    {1u, kEtcPalIpTypeV4, "10.101.20.30", "255.255.0.0", 0, "00:c0:16:22:22:22", "eth_v4_0", true, false},
    {2u, kEtcPalIpTypeV6, "fe80::1234", "", 64u, "00:c0:16:33:33:33", "eth_v6_0", false, false},
};

typedef struct UnicastInfo
{
  etcpal_iptype_t type;
  std::string addr_string;
  bool got_universe_data;
} UnicastInfo;

std::vector<UnicastInfo> fake_unicasts_info = {
    {kEtcPalIpTypeV4, "10.101.20.1", false},
    {kEtcPalIpTypeV4, "10.101.20.2", false},
};

// clang-format off
std::vector<uint8_t> test_levels_data =
{
  0x00, 0x10,                                                                                      // size of preamble
  0x00, 0x00,                                                                                      // size of postamble
  0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00,                          // "ASC-E1.17"
  0x72, 0x6e,                                                                                      // pdu flags & length
  0x00, 0x00, 0x00, 0x04,                                                                          // ratified dmx protocol
  0x7b, 0x39, 0x63, 0x38, 0x39, 0x64, 0x65, 0x36, 0x62, 0x2d, 0x65, 0x37, 0x35, 0x37, 0x2d, 0x34,  // CID 7b396338-3964-6536-622d-653735372d34
  0x72, 0x58,                                                                                      // pdu flags & length
  0x00, 0x00, 0x00, 0x02,                                                                          // streaming dmx
  0x73, 0x41, 0x43, 0x4e, 0x56, 0x69, 0x65, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // source "sACNView"
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x64,                                                                                            // priority 100
  0x00, 0x00,                                                                                      // reserved
  0x22,                                                                                            // sequence number
  0x00,                                                                                            // options (preview & stream terminated bits)
  0x00, 0x01,                                                                                      // universe
  0x72, 0x0b,                                                                                      // pdu flags & length
  0x02,                                                                                            // set property
  0xa1,                                                                                            // address & data type
  0x00, 0x00,                                                                                      // first address
  0x00, 0x01,                                                                                      // increment
  0x02, 0x01,                                                                                      // count 513
  SACN_STARTCODE_DMX,                                                                              // start code
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // data
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 5
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 10
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 15
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 20
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 25
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 30
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

std::vector<uint8_t> test_pap_data =
{
  0x00, 0x10,                                                                                      // size of preamble
  0x00, 0x00,                                                                                      // size of postamble
  0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00,                          // "ASC-E1.17"
  0x72, 0x6e,                                                                                      // pdu flags & length
  0x00, 0x00, 0x00, 0x04,                                                                          // ratified dmx protocol
  0x7b, 0x39, 0x63, 0x38, 0x39, 0x64, 0x65, 0x36, 0x62, 0x2d, 0x65, 0x37, 0x35, 0x37, 0x2d, 0x34,  // CID 7b396338-3964-6536-622d-653735372d34
  0x72, 0x58,                                                                                      // pdu flags & length
  0x00, 0x00, 0x00, 0x02,                                                                          // streaming dmx
  0x73, 0x41, 0x43, 0x4e, 0x56, 0x69, 0x65, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // source "sACNView"
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x64,                                                                                            // priority 100
  0x00, 0x00,                                                                                      // reserved
  0x22,                                                                                            // sequence number
  0x00,                                                                                            // options (preview & stream terminated bits)
  0x00, 0x01,                                                                                      // universe
  0x72, 0x0b,                                                                                      // pdu flags & length
  0x02,                                                                                            // set property
  0xa1,                                                                                            // address & data type
  0x00, 0x00,                                                                                      // first address
  0x00, 0x01,                                                                                      // increment
  0x02, 0x01,                                                                                      // count 513
  SACN_STARTCODE_PRIORITY,                                                                         // start code
  0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // data
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 5
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 10
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 15
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 20
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 25
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 30
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clang-format on

static bool received_levels_data = false;
static bool received_pap_data = false;
static bool is_sampling = false;

class TestReceiverNotifyHandler : public Receiver::NotifyHandler
{
public:
  void HandleUniverseData(Receiver::Handle receiver_handle, const etcpal::SockAddr& source_addr,
                          const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data) override
  {
    ETCPAL_UNUSED_ARG(receiver_handle);
    ETCPAL_UNUSED_ARG(source_info);

    std::string source_addr_str = source_addr.ip().ToString();

    for (auto& fake_network_info : fake_networks_info)
    {
      if (fake_network_info.addr == source_addr_str)
      {
        fake_network_info.got_universe_data = true;
        break;
      }
    }
    for (auto& fake_unicast_info : fake_unicasts_info)
    {
      if (fake_unicast_info.addr_string == source_addr_str)
      {
        fake_unicast_info.got_universe_data = true;
        break;
      }
    }

    if (universe_data.start_code == SACN_STARTCODE_DMX)
    {
      received_levels_data = true;
    }
    else if (universe_data.start_code == SACN_STARTCODE_PRIORITY)
    {
      received_pap_data = true;
    }
  }

  void HandleSourcesLost(Receiver::Handle handle, uint16_t universe,
                         const std::vector<SacnLostSource>& lost_sources) override
  {
    ETCPAL_UNUSED_ARG(handle);
    ETCPAL_UNUSED_ARG(lost_sources);
    ETCPAL_UNUSED_ARG(universe);
  }

  void HandleSamplingPeriodStarted(Receiver::Handle handle, uint16_t universe) override
  {
    ETCPAL_UNUSED_ARG(handle);
    ETCPAL_UNUSED_ARG(universe);
    is_sampling = true;
  }

  void HandleSamplingPeriodEnded(Receiver::Handle handle, uint16_t universe) override
  {
    ETCPAL_UNUSED_ARG(handle);
    ETCPAL_UNUSED_ARG(universe);
    is_sampling = false;
  }
};

class MockMergeReceiverNotifyHandler : public MergeReceiver::NotifyHandler
{
public:
  MockMergeReceiverNotifyHandler() = default;

  MOCK_METHOD(void, HandleMergedData, (MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data),
              (override));
};

class TestReceiverBase : public ::testing::Test
{
protected:
  enum class FakeReceiveMode
  {
    kMulticast,
    kUnicast
  };

  enum class FakeReceiveFlags
  {
    kTerminate,
    kNoTermination
  };

  void SetUp() override
  {
    etcpal_reset_all_fakes();

    PopulateFakeNetints();

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

    etcpal_cmsg_to_pktinfo_fake.custom_fake = [](const EtcPalCMsgHdr*, EtcPalPktInfo*) { return true; };

    etcpal_cmsg_firsthdr_fake.custom_fake = [](EtcPalMsgHdr*, EtcPalCMsgHdr*) { return true; };

    etcpal_poll_wait_fake.custom_fake = [](EtcPalPollContext*, EtcPalPollEvent* event, int) {
      event->socket = (next_socket - 1);
      event->events = ETCPAL_POLL_IN;
      return kEtcPalErrOk;
    };

    etcpal_netint_get_interfaces_fake.custom_fake = [](EtcPalNetintInfo* netints, size_t* num_netints) {
      auto result = validate_get_interfaces_args(netints, num_netints);
      if (result != kEtcPalErrOk)
        return result;

      return copy_out_interfaces(fake_sys_netints_.data(), fake_sys_netints_.size(), netints, num_netints);
    };

    etcpal_socket_fake.custom_fake = [](unsigned int, unsigned int, etcpal_socket_t* new_sock) {
      EXPECT_NE(new_sock, nullptr);
      *new_sock = next_socket++;
      return kEtcPalErrOk;
    };

    etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr*, int) {
      return static_cast<int>(kEtcPalErrTimedOut);
    };

    ResetNotifyVariables();

    ASSERT_EQ(Init().code(), kEtcPalErrOk);
    is_sampling = false;
  }

  static int FakeReceive(FakeReceiveMode mode, uint8_t index, const std::vector<uint8_t>& data, EtcPalMsgHdr* msg,
                         const etcpal::Uuid& source_cid = etcpal::Uuid(),
                         FakeReceiveFlags flags = FakeReceiveFlags::kNoTermination)
  {
    EXPECT_NE(msg, nullptr);
    EtcPalSockAddr etcpal_sock_addr;
    EtcPalIpAddr ip;
    if (mode == FakeReceiveMode::kMulticast)
    {
      auto& fake_network_info = fake_networks_info[index];
      etcpal_string_to_ip(fake_network_info.type, fake_network_info.addr.c_str(), &ip);
    }
    else
    {
      auto& fake_unicast_info = fake_unicasts_info[index];
      etcpal_string_to_ip(fake_unicast_info.type, fake_unicast_info.addr_string.c_str(), &ip);
    }
    etcpal_sock_addr.ip = ip;
    etcpal_sock_addr.port = 0;
    msg->flags = 0;
    msg->name = etcpal_sock_addr;

    char* msg_buf = reinterpret_cast<char*>(msg->buf);
    memcpy(msg_buf, data.data(), data.size());
    if (!source_cid.IsNull())
      memcpy(&msg_buf[kCidOffset], source_cid.data(), ETCPAL_UUID_BYTES);
    if (flags == FakeReceiveFlags::kTerminate)
      msg_buf[kOptionsOffset] |= SACN_OPTVAL_TERMINATED;

    msg->buflen = data.size();
    return (int)data.size();
  }

  static std::vector<uint8_t> CustomLevelData(const std::vector<uint8_t>& levels)
  {
    std::vector<uint8_t> res = test_levels_data;
    for (int i = 0; i < levels.size(); ++i)
      res[kSlotsOffset + i] = levels[i];

    return res;
  }

  static std::vector<uint8_t> CustomPapData(const std::vector<uint8_t>& paps)
  {
    std::vector<uint8_t> res = test_pap_data;
    for (int i = 0; i < paps.size(); ++i)
      res[kSlotsOffset + i] = paps[i];

    return res;
  }

  void PopulateFakeNetints()
  {
    EtcPalNetintInfo fake_netint;
    for (auto fake_network_info : fake_networks_info)
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
      fake_sys_netints_.push_back(fake_netint);
    }
  }

  void ResetNotifyVariables()
  {
    received_levels_data = false;
    received_pap_data = false;
    for (auto& fake_network_info : fake_networks_info)
    {
      fake_network_info.got_universe_data = false;
    }
    for (auto& fake_unicast_info : fake_unicasts_info)
    {
      fake_unicast_info.got_universe_data = false;
    }
  }

  void RunThreadCycle(bool increment_sequence_num)
  {
    sacn_thread_id_t thread_id = 0;
    SacnRecvThreadContext* recv_thread_context = get_recv_thread_context(thread_id);
    read_network_and_process(recv_thread_context);

    if (increment_sequence_num)
    {
      ++seq_num_;
      test_levels_data[SACN_SEQ_OFFSET] = seq_num_;
      test_pap_data[SACN_SEQ_OFFSET] = seq_num_;
    }
  }

  void TearDown() override { Deinit(); }

  static std::vector<EtcPalNetintInfo> fake_sys_netints_;
  static uint8_t seq_num_;
};

std::vector<EtcPalNetintInfo> TestReceiverBase::fake_sys_netints_;
uint8_t TestReceiverBase::seq_num_ = 0u;

class TestReceiver : public TestReceiverBase
{
protected:
  void SetUp() override
  {
    TestReceiverBase::SetUp();

    EXPECT_EQ(receiver_.Startup(receiver_settings_, notify_handler_), kEtcPalErrOk);
  }

  void TearDown() override
  {
    receiver_.Shutdown();

    TestReceiverBase::TearDown();
  }

  Receiver receiver_;
  Receiver::Settings receiver_settings_{kTestUniverse};
  TestReceiverNotifyHandler notify_handler_;
};

class TestMergeReceiver : public TestReceiverBase
{
protected:
  void SetUp() override
  {
    TestReceiverBase::SetUp();

    EXPECT_EQ(merge_receiver_.Startup(merge_receiver_settings_, mock_notify_handler_), kEtcPalErrOk);
  }

  void TearDown() override
  {
    merge_receiver_.Shutdown();

    TestReceiverBase::TearDown();
  }

  MergeReceiver merge_receiver_;
  MergeReceiver::Settings merge_receiver_settings_{kTestUniverse};
  NiceMock<MockMergeReceiverNotifyHandler> mock_notify_handler_;
};

MATCHER_P(ControlsLevels, expected_levels, "")  // NOLINT(readability-redundant-string-init)
{
  ETCPAL_UNUSED_ARG(result_listener);

  for (int i = 0; i < arg.slot_range.address_count; ++i)
  {
    if (i < expected_levels.size())
    {
      if ((arg.levels[i] != expected_levels[i]) || (arg.priorities[i] == 0u))
        return false;
    }
    else if ((arg.levels[i] != 0u) || (arg.priorities[i] != 0u))
    {
      return false;
    }
  }

  return true;
}

/*===========================================================================*/

TEST_F(TestReceiver, SamplingPeriod)
{
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg);
  };
  is_sampling = false;
  RunThreadCycle(true);
  etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
  RunThreadCycle(true);
  EXPECT_TRUE(is_sampling);
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle(true);
  EXPECT_FALSE(is_sampling);
}

TEST_F(TestReceiver, ReceivePap)
{
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg);
  };
  RunThreadCycle(true);
  EXPECT_TRUE(received_levels_data);
  EXPECT_FALSE(received_pap_data);

  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, test_pap_data, msg);
  };
  received_levels_data = false;
  received_pap_data = false;
  RunThreadCycle(true);
  EXPECT_FALSE(received_levels_data);
  EXPECT_TRUE(received_pap_data);
}

TEST_F(TestReceiver, Ipv4Ipv6)
{
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg);
  };
  RunThreadCycle(true);
  EXPECT_TRUE(fake_networks_info[0].got_universe_data);

  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 1, test_levels_data, msg);
  };
  fake_networks_info[1].got_universe_data = false;
  RunThreadCycle(true);
  EXPECT_TRUE(fake_networks_info[1].got_universe_data);
}

TEST_F(TestReceiver, SamePacketIpv4Ipv6)
{
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg);
  };
  RunThreadCycle(false);
  EXPECT_TRUE(fake_networks_info[0].got_universe_data);

  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 1, test_levels_data, msg);
  };
  fake_networks_info[1].got_universe_data = false;
  RunThreadCycle(true);
  EXPECT_FALSE(fake_networks_info[1].got_universe_data);
}

TEST_F(TestReceiver, MulticastAndUnicast)
{
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg);
  };
  RunThreadCycle(true);
  EXPECT_TRUE(fake_networks_info[0].got_universe_data);

  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kUnicast, 0, test_levels_data, msg);
  };
  fake_unicasts_info[0].got_universe_data = false;
  RunThreadCycle(true);
  EXPECT_TRUE(fake_unicasts_info[0].got_universe_data);
}

TEST_F(TestMergeReceiver, HandlesSameSourceReappearing)
{
  static constexpr int kNumIterations = 0x10000;  // Cause 16-bit source handles to wrap around
  static etcpal::Uuid source_cid;

  // Elapse sampling period
  RunThreadCycle(false);
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle(false);

  // New source
  source_cid = etcpal::Uuid::V4();

  for (int i = 0; i < kNumIterations; ++i)
  {
    // Data packet
    etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
      return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg, source_cid,
                         FakeReceiveFlags::kNoTermination);
    };
    RunThreadCycle(true);

    // Termination packet
    etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
      return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg, source_cid,
                         FakeReceiveFlags::kTerminate);
    };
    etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
    RunThreadCycle(true);
  }
}

TEST_F(TestMergeReceiver, HandlesManySourcesAppearing)
{
  static constexpr int kNumIterations = 0x10000;  // Cause 16-bit source handles to wrap around
  static etcpal::Uuid source_cid;

  // Elapse sampling period
  RunThreadCycle(false);
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle(false);

  for (int i = 0; i < kNumIterations; ++i)
  {
    // New source
    source_cid = etcpal::Uuid::V4();

    // Data packet
    etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
      return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg, source_cid,
                         FakeReceiveFlags::kNoTermination);
    };
    RunThreadCycle(true);

    // Termination packet
    etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
      return FakeReceive(FakeReceiveMode::kMulticast, 0, test_levels_data, msg, source_cid,
                         FakeReceiveFlags::kTerminate);
    };
    etcpal_getms_fake.return_val += (SACN_PERIODIC_INTERVAL + 1u);
    RunThreadCycle(true);
  }
}

TEST_F(TestMergeReceiver, MergesInitialPapPacketDuringSampling)
{
  static const std::vector<uint8_t> kTestLevelData = {255u, 255u, 255u, 255u, 255u, 255u};
  static const std::vector<uint8_t> kTestPapData = {100u, 100u, 100u, 100u, 100u, 100u};

  // Begin sampling period
  RunThreadCycle(false);

  // PAP arrives first
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomPapData(kTestPapData), msg);
  };
  RunThreadCycle(true);

  // Followed by levels
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomLevelData(kTestLevelData), msg);
  };
  RunThreadCycle(true);

  // Sampling period ends, merged data callback fires
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kTestLevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr*, int) {
    return static_cast<int>(kEtcPalErrTimedOut);
  };
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle(false);

  // PAP comes in again, firing merged data callback once more
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kTestLevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomPapData(kTestPapData), msg);
  };
  RunThreadCycle(true);
}

TEST_F(TestMergeReceiver, MergesInitialLevelsPacketDuringSampling)
{
  static const std::vector<uint8_t> kTestLevelData = {255u, 255u, 255u, 255u, 255u, 255u};
  static const std::vector<uint8_t> kTestPapData = {100u, 100u, 100u, 100u, 100u, 100u};

  // Begin sampling period
  RunThreadCycle(false);

  // Levels arrive first
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomLevelData(kTestLevelData), msg);
  };
  RunThreadCycle(true);

  // Followed by PAP
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomPapData(kTestPapData), msg);
  };
  RunThreadCycle(true);

  // Sampling period ends, merged data callback fires
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kTestLevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr*, int) {
    return static_cast<int>(kEtcPalErrTimedOut);
  };
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle(false);

  // Levels come in again, firing merged data callback once more
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kTestLevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomLevelData(kTestLevelData), msg);
  };
  RunThreadCycle(true);
}

TEST_F(TestMergeReceiver, InitialPapDoesNotMergeUntilLevelsArrive)
{
  static const std::vector<uint8_t> kEmptyLevelData = {};
  static const std::vector<uint8_t> kSrc1LevelData = {255u, 255u, 255u, 255u, 255u, 255u};
  static const std::vector<uint8_t> kSrc1PapData = {100u, 100u, 100u, 100u, 100u, 100u};
  static const std::vector<uint8_t> kSrc2LevelData = {0u, 0u, 0u, 0u, 0u, 0u};
  static const std::vector<uint8_t> kSrc2PapData = {200u, 200u, 200u, 200u, 200u, 200u};

  static etcpal::Uuid source_1_cid = etcpal::Uuid::V4();
  static etcpal::Uuid source_2_cid = etcpal::Uuid::V4();

  // Elapse sampling period
  RunThreadCycle(false);
  etcpal_getms_fake.return_val += (SACN_SAMPLE_TIME + 1u);
  RunThreadCycle(false);

  // Source 1 0xDD received - expect empty merge results
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kEmptyLevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomPapData(kSrc1PapData), msg, source_1_cid);
  };
  RunThreadCycle(true);

  // Source 1 0x00 received - should be seen in merge now
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kSrc1LevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomLevelData(kSrc1LevelData), msg, source_1_cid);
  };
  RunThreadCycle(true);

  // Source 2 0xDD received - merge should be unaffected
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kSrc1LevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomPapData(kSrc2PapData), msg, source_2_cid);
  };
  RunThreadCycle(true);

  // Source 2 0x00 received - merge should be affected
  EXPECT_CALL(mock_notify_handler_, HandleMergedData(_, ControlsLevels(kSrc2LevelData))).Times(1);
  etcpal_recvmsg_fake.custom_fake = [](etcpal_socket_t, EtcPalMsgHdr* msg, int) {
    return FakeReceive(FakeReceiveMode::kMulticast, 0, CustomLevelData(kSrc2LevelData), msg, source_2_cid);
  };
  RunThreadCycle(true);
}
