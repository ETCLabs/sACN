# sACN merge receiver disabled configuration integration tests

set(TEST_MERGE_RECEIVER_DISABLED_SOURCES
  test_merge_receiver_disabled.cpp
  main.cpp

  ${SACN_SOURCES}
)

sacn_add_test(integration_test_merge_receiver_disabled_dynamic ${SACN_TEST}/configs/merge_receiver_disabled_dynamic ${TEST_MERGE_RECEIVER_DISABLED_SOURCES})
sacn_add_test(integration_test_merge_receiver_disabled_static ${SACN_TEST}/configs/merge_receiver_disabled_static ${TEST_MERGE_RECEIVER_DISABLED_SOURCES})
