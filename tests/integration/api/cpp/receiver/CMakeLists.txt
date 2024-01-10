# sACN C++ Receiver API integration tests

set(TEST_CPP_RECEIVER_SOURCES
  test_cpp_receiver.cpp
  main.cpp
  
  ${SACN_SOURCES}
)

sacn_add_dynamic_test(test_cpp_receiver ${TEST_CPP_RECEIVER_SOURCES})
sacn_add_static_test(test_cpp_receiver ${TEST_CPP_RECEIVER_SOURCES})
