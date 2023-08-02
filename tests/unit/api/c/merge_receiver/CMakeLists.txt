# sACN C Merge Receiver API unit tests

set(TEST_MERGE_RECEIVER_SOURCES
  test_merge_receiver.cpp
  main.cpp

  ${SACN_MEM_SOURCES}
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/merge_receiver.c
  ${SACN_SRC}/sacn/util.c
  
  ${SACN_SRC}/sacn_mock/dmx_merger.c
  ${SACN_SRC}/sacn_mock/receiver.c
  
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_merge_receiver ${TEST_MERGE_RECEIVER_SOURCES})
sacn_add_static_test(test_merge_receiver ${TEST_MERGE_RECEIVER_SOURCES})
sacn_add_test(unit_test_merge_receiver_sampling_merger_disabled_dynamic ${SACN_TEST}/configs/merge_receiver_sampling_merger_disabled_dynamic ${TEST_MERGE_RECEIVER_SOURCES})
sacn_add_test(unit_test_merge_receiver_sampling_merger_disabled_static ${SACN_TEST}/configs/merge_receiver_sampling_merger_disabled_static ${TEST_MERGE_RECEIVER_SOURCES})
