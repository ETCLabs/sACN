# sACN C Merge Receiver API unit tests

set(TEST_MERGE_RECEIVER_SOURCES
  test_merge_receiver.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_merge_receiver ${SACN_TEST}/configs/dynamic ${TEST_MERGE_RECEIVER_SOURCES})
sacn_add_static_test(test_merge_receiver ${SACN_TEST}/configs/static ${TEST_MERGE_RECEIVER_SOURCES})
