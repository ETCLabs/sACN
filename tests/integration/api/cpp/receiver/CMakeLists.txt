# sACN C++ Receiver API integration tests

set(TEST_CPP_RECEIVER_SOURCES
  test_cpp_receiver.cpp
  main.cpp
  
  ${SACN_SOURCES}
)

sacn_add_dynamic_test(test_cpp_receiver ${TEST_CPP_RECEIVER_SOURCES})
sacn_add_static_test(test_cpp_receiver ${TEST_CPP_RECEIVER_SOURCES})
sacn_add_test(integration_test_cpp_receiver_larger_max_slots_dynamic ${SACN_TEST}/configs/merge_receiver_larger_max_slots_dynamic ${TEST_CPP_RECEIVER_SOURCES})
sacn_add_test(integration_test_cpp_receiver_larger_max_slots_static ${SACN_TEST}/configs/merge_receiver_larger_max_slots_static ${TEST_CPP_RECEIVER_SOURCES})
sacn_add_test(integration_test_cpp_receiver_smaller_max_slots_dynamic ${SACN_TEST}/configs/merge_receiver_smaller_max_slots_dynamic ${TEST_CPP_RECEIVER_SOURCES})
sacn_add_test(integration_test_cpp_receiver_smaller_max_slots_static ${SACN_TEST}/configs/merge_receiver_smaller_max_slots_static ${TEST_CPP_RECEIVER_SOURCES})
