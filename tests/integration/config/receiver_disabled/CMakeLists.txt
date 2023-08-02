# sACN receiver disabled configuration integration tests

set(TEST_RECEIVER_DISABLED_SOURCES
  test_receiver_disabled.cpp
  main.cpp

  ${SACN_SOURCES}
)

sacn_add_test(integration_test_receiver_disabled_dynamic ${SACN_TEST}/configs/receiver_disabled_dynamic ${TEST_RECEIVER_DISABLED_SOURCES})
sacn_add_test(integration_test_receiver_disabled_static ${SACN_TEST}/configs/receiver_disabled_static ${TEST_RECEIVER_DISABLED_SOURCES})
