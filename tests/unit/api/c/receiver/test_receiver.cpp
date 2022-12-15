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

#include "sacn/receiver.h"

#include <limits>
#include "etcpal_mock/common.h"
#include "sacn_mock/private/common.h"
#include "sacn_mock/private/receiver_state.h"
#include "sacn_mock/private/source_loss.h"
#include "sacn_mock/private/sockets.h"
#include "sacn/private/mem.h"
#include "sacn/private/opts.h"
#include "sacn/private/receiver.h"
#include "gtest/gtest.h"
#include "fff.h"

#if SACN_DYNAMIC_MEM
#define TestReceiver TestReceiverDynamic
#else
#define TestReceiver TestReceiverStatic
#endif

constexpr uint16_t CHANGE_UNIVERSE_WORKS_FIRST_IPV4_SOCKET = 4u;
constexpr uint16_t CHANGE_UNIVERSE_WORKS_FIRST_IPV6_SOCKET = 6u;
constexpr uint16_t CHANGE_UNIVERSE_WORKS_FIRST_UNIVERSE = 1u;
constexpr uint16_t CHANGE_UNIVERSE_WORKS_SECOND_UNIVERSE = 2u;
constexpr uint16_t CHANGE_UNIVERSE_INVALID_UNIVERSE_1 = 0u;
constexpr uint16_t CHANGE_UNIVERSE_INVALID_UNIVERSE_2 = 64001u;
constexpr uint16_t CHANGE_UNIVERSE_VALID_UNIVERSE_1 = 1u;
constexpr uint16_t CHANGE_UNIVERSE_VALID_UNIVERSE_2 = 1u;
constexpr uint16_t CHANGE_UNIVERSE_NO_RECEIVER_UNIVERSE_1 = 1u;
constexpr uint16_t CHANGE_UNIVERSE_NO_RECEIVER_UNIVERSE_2 = 2u;
constexpr uint16_t CHANGE_UNIVERSE_RECEIVER_EXISTS_UNIVERSE = 7u;

class TestReceiver : public ::testing::Test
{
protected:
  void SetUp() override
  {
    etcpal_reset_all_fakes();
    sacn_common_reset_all_fakes();
    sacn_source_loss_reset_all_fakes();
    sacn_sockets_reset_all_fakes();

    sacn_initialize_receiver_netints_fake.custom_fake = [](SacnInternalNetintArray* internal_netints, bool,
                                                           EtcPalRbTree*, const SacnNetintConfig*) {
#if SACN_DYNAMIC_MEM
      internal_netints->netints = NULL;
      internal_netints->netints_capacity = 0;
#endif
      internal_netints->num_netints = 0;
      return kEtcPalErrOk;
    };

    ASSERT_EQ(sacn_receiver_mem_init(1), kEtcPalErrOk);
    ASSERT_EQ(sacn_receiver_init(), kEtcPalErrOk);
  }

  void TearDown() override
  {
    sacn_receiver_deinit();
    sacn_receiver_mem_deinit();
  }

  sacn_receiver_t SetupUniverseChangeTest(sacn_ip_support_t ip_supported)
  {
    SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
    config.callbacks.universe_data = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                        const SacnRecvUniverseData*, void*) {};
    config.callbacks.sources_lost = [](sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*) {};
    config.callbacks.sampling_period_ended = [](sacn_receiver_t, uint16_t, void*) {};
    config.universe_id = CHANGE_UNIVERSE_WORKS_FIRST_UNIVERSE;
    config.ip_supported = ip_supported;

    sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t, etcpal_iptype_t ip_type, uint16_t,
                                                   const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
      if (ip_type == kEtcPalIpTypeV4)
        *socket = CHANGE_UNIVERSE_WORKS_FIRST_IPV4_SOCKET;
      else if (ip_type == kEtcPalIpTypeV6)
        *socket = CHANGE_UNIVERSE_WORKS_FIRST_IPV6_SOCKET;
      return kEtcPalErrOk;
    };

    sacn_receiver_t handle;
    sacn_receiver_create(&config, &handle, nullptr);

    clear_term_set_list_fake.custom_fake = [](TerminationSet* list) { EXPECT_EQ(list, nullptr); };

    return handle;
  }

  void PerformUniverseChangeTest(sacn_receiver_t handle, sacn_ip_support_t ip_supported)
  {
    etcpal_error_t change_universe_result =
        sacn_receiver_change_universe(handle, CHANGE_UNIVERSE_WORKS_SECOND_UNIVERSE);

    EXPECT_EQ(change_universe_result, kEtcPalErrOk);
    EXPECT_EQ(sacn_lock_fake.call_count, sacn_unlock_fake.call_count);
    EXPECT_EQ(sacn_initialized_fake.call_count, 2u);
    EXPECT_EQ(clear_term_set_list_fake.call_count, 1u);

    switch (ip_supported)
    {
      case kSacnIpV4Only:
      case kSacnIpV6Only:
        EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 1u);
        EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 2u);
        break;
      case kSacnIpV4AndIpV6:
        EXPECT_EQ(sacn_remove_receiver_socket_fake.call_count, 2u);
        EXPECT_EQ(sacn_add_receiver_socket_fake.call_count, 4u);
    }

    sacn_receiver_destroy(handle);
  }
};

TEST_F(TestReceiver, SetExpiredWaitWorks)
{
  // TODO: Refactor into test_receiver_state

  //// Initialization should set it to the default
  // EXPECT_EQ(sacn_receiver_get_expired_wait(), SACN_DEFAULT_EXPIRED_WAIT_MS);

  // sacn_receiver_set_expired_wait(0);
  // EXPECT_EQ(sacn_receiver_get_expired_wait(), 0u);
  // sacn_receiver_set_expired_wait(5000);
  // EXPECT_EQ(sacn_receiver_get_expired_wait(), 5000u);
  // sacn_receiver_set_expired_wait(std::numeric_limits<uint32_t>::max());
  // EXPECT_EQ(sacn_receiver_get_expired_wait(), std::numeric_limits<uint32_t>::max());
}

TEST_F(TestReceiver, ChangeUniverseV4Works)
{
  // TODO: Refactor into test_receiver_state

  // sacn_receiver_t handle = SetupUniverseChangeTest(kSacnIpV4Only);

  // sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket,
  //                                                   socket_cleanup_behavior_t) {
  //   ASSERT_NE(socket, nullptr);
  //   EXPECT_NE(*socket, CHANGE_UNIVERSE_WORKS_FIRST_IPV6_SOCKET);
  //   EXPECT_NE(get_recv_thread_context(thread_id), nullptr);
  // };
  // sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t
  // universe,
  //                                                const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
  //   EXPECT_NE(ip_type, kEtcPalIpTypeV6);
  //   EXPECT_NE(ip_type, kEtcPalIpTypeInvalid);
  //   EXPECT_EQ(universe, CHANGE_UNIVERSE_WORKS_SECOND_UNIVERSE);
  //   EXPECT_NE(get_recv_thread_context(thread_id), nullptr);
  //   EXPECT_NE(socket, nullptr);
  //   return kEtcPalErrOk;
  // };

  // PerformUniverseChangeTest(handle, kSacnIpV4Only);
}

TEST_F(TestReceiver, ChangeUniverseV6Works)
{
  // TODO: Refactor into test_receiver_state

  // sacn_receiver_t handle = SetupUniverseChangeTest(kSacnIpV6Only);

  // sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket,
  //                                                   socket_cleanup_behavior_t) {
  //   ASSERT_NE(socket, nullptr);
  //   EXPECT_NE(*socket, CHANGE_UNIVERSE_WORKS_FIRST_IPV4_SOCKET);
  //   EXPECT_NE(get_recv_thread_context(thread_id), nullptr);
  // };
  // sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t
  // universe,
  //                                                const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
  //   EXPECT_NE(ip_type, kEtcPalIpTypeV4);
  //   EXPECT_NE(ip_type, kEtcPalIpTypeInvalid);
  //   EXPECT_EQ(universe, CHANGE_UNIVERSE_WORKS_SECOND_UNIVERSE);
  //   EXPECT_NE(get_recv_thread_context(thread_id), nullptr);
  //   EXPECT_NE(socket, nullptr);
  //   return kEtcPalErrOk;
  // };

  // PerformUniverseChangeTest(handle, kSacnIpV6Only);
}

TEST_F(TestReceiver, ChangeUniverseV4V6Works)
{
  // TODO: Refactor into test_receiver_state

  // sacn_receiver_t handle = SetupUniverseChangeTest(kSacnIpV4AndIpV6);

  // sacn_remove_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_socket_t* socket,
  //                                                   socket_cleanup_behavior_t) {
  //   ASSERT_NE(socket, nullptr);
  //   EXPECT_NE(get_recv_thread_context(thread_id), nullptr);
  // };
  // sacn_add_receiver_socket_fake.custom_fake = [](sacn_thread_id_t thread_id, etcpal_iptype_t ip_type, uint16_t
  // universe,
  //                                                const EtcPalMcastNetintId*, size_t, etcpal_socket_t* socket) {
  //   EXPECT_NE(ip_type, kEtcPalIpTypeInvalid);
  //   EXPECT_EQ(universe, CHANGE_UNIVERSE_WORKS_SECOND_UNIVERSE);
  //   EXPECT_NE(get_recv_thread_context(thread_id), nullptr);
  //   EXPECT_NE(socket, nullptr);
  //   return kEtcPalErrOk;
  // };

  // PerformUniverseChangeTest(handle, kSacnIpV4AndIpV6);
}

TEST_F(TestReceiver, ChangeUniverseErrInvalidWorks)
{
  etcpal_error_t change_universe_invalid_result_1 =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_INVALID_UNIVERSE_1);
  EXPECT_EQ(change_universe_invalid_result_1, kEtcPalErrInvalid);

  etcpal_error_t change_universe_invalid_result_2 =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_INVALID_UNIVERSE_2);
  EXPECT_EQ(change_universe_invalid_result_2, kEtcPalErrInvalid);

  etcpal_error_t change_universe_valid_result =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_VALID_UNIVERSE_1);
  EXPECT_NE(change_universe_valid_result, kEtcPalErrInvalid);
}

TEST_F(TestReceiver, ChangeUniverseErrNotInitWorks)
{
  sacn_initialized_fake.return_val = false;

  etcpal_error_t change_universe_not_init_result =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_VALID_UNIVERSE_1);
  EXPECT_EQ(change_universe_not_init_result, kEtcPalErrNotInit);

  sacn_initialized_fake.return_val = true;

  etcpal_error_t change_universe_init_result =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_VALID_UNIVERSE_1);
  EXPECT_NE(change_universe_init_result, kEtcPalErrNotInit);
}

TEST_F(TestReceiver, ChangeUniverseErrExistsWorks)
{
  SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
  config.callbacks.universe_data = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData*, void*) {};
  config.callbacks.sources_lost = [](sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*) {};
  config.callbacks.sampling_period_ended = [](sacn_receiver_t, uint16_t, void*) {};

  config.universe_id = CHANGE_UNIVERSE_RECEIVER_EXISTS_UNIVERSE;

  sacn_receiver_t handle_existing_receiver;
  sacn_receiver_create(&config, &handle_existing_receiver, nullptr);

  config.universe_id = CHANGE_UNIVERSE_NO_RECEIVER_UNIVERSE_1;
  ++get_next_receiver_handle_fake.return_val;

  sacn_receiver_t handle_changing_receiver;
  sacn_receiver_create(&config, &handle_changing_receiver, nullptr);

  etcpal_error_t change_universe_no_err_exists_result =
      sacn_receiver_change_universe(handle_changing_receiver, CHANGE_UNIVERSE_NO_RECEIVER_UNIVERSE_2);
  EXPECT_EQ(change_universe_no_err_exists_result, kEtcPalErrOk);

  etcpal_error_t change_universe_err_exists_result =
      sacn_receiver_change_universe(handle_changing_receiver, CHANGE_UNIVERSE_RECEIVER_EXISTS_UNIVERSE);
  EXPECT_EQ(change_universe_err_exists_result, kEtcPalErrExists);
}

TEST_F(TestReceiver, ChangeUniverseErrNotFoundWorks)
{
  etcpal_error_t change_universe_not_found_result =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_VALID_UNIVERSE_2);
  EXPECT_EQ(change_universe_not_found_result, kEtcPalErrNotFound);

  SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
  config.callbacks.universe_data = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData*, void*) {};
  config.callbacks.sources_lost = [](sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*) {};
  config.callbacks.sampling_period_ended = [](sacn_receiver_t, uint16_t, void*) {};
  config.universe_id = CHANGE_UNIVERSE_VALID_UNIVERSE_1;

  sacn_receiver_t handle;
  sacn_receiver_create(&config, &handle, nullptr);

  etcpal_error_t change_universe_found_result = sacn_receiver_change_universe(handle, CHANGE_UNIVERSE_VALID_UNIVERSE_2);
  EXPECT_NE(change_universe_found_result, kEtcPalErrNotFound);
}

TEST_F(TestReceiver, ChangeUniverseErrSysWorks)
{
  sacn_lock_fake.return_val = false;

  etcpal_error_t change_universe_err_sys_result =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_VALID_UNIVERSE_1);
  EXPECT_EQ(change_universe_err_sys_result, kEtcPalErrSys);

  sacn_lock_fake.return_val = true;

  etcpal_error_t change_universe_no_err_sys_result =
      sacn_receiver_change_universe(sacn_receiver_t(), CHANGE_UNIVERSE_VALID_UNIVERSE_1);
  EXPECT_NE(change_universe_no_err_sys_result, kEtcPalErrSys);
}

TEST_F(TestReceiver, ResetNetworkingTerminatesSourcesOnLostNetints)
{
  SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
  config.callbacks.universe_data = [](sacn_receiver_t, const EtcPalSockAddr*, const SacnRemoteSource*,
                                      const SacnRecvUniverseData*, void*) {};
  config.callbacks.sources_lost = [](sacn_receiver_t, uint16_t, const SacnLostSource*, size_t, void*) {};
  config.callbacks.sampling_period_ended = [](sacn_receiver_t, uint16_t, void*) {};

  config.universe_id = CHANGE_UNIVERSE_RECEIVER_EXISTS_UNIVERSE;

  sacn_receiver_t handle;
  sacn_receiver_create(&config, &handle, nullptr);

  SacnNetintConfig sys_netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
  EXPECT_EQ(terminate_sources_on_removed_netints_fake.call_count, 0u);
  sacn_receiver_reset_networking(&sys_netint_config);
  EXPECT_EQ(terminate_sources_on_removed_netints_fake.call_count, 1u);
  SacnReceiverNetintList receiver_netint_list = {handle, nullptr, 0u, false};
  sacn_receiver_reset_networking_per_receiver(&sys_netint_config, &receiver_netint_list, 1u);
  EXPECT_EQ(terminate_sources_on_removed_netints_fake.call_count, 2u);
}
