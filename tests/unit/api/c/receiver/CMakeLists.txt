# sACN C Receiver API unit tests

set(TEST_RECEIVER_SOURCES
  test_receiver.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_receiver ${TEST_RECEIVER_SOURCES})
sacn_add_static_test(test_receiver ${TEST_RECEIVER_SOURCES})
