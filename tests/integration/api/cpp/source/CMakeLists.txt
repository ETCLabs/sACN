# sACN C++ Source API integration tests

set(TEST_SOURCE_SOURCES
  test_cpp_source.cpp
  main.cpp
  
  ${SACN_SOURCES}
)

sacn_add_dynamic_test(test_cpp_source ${TEST_SOURCE_SOURCES})
sacn_add_static_test(test_cpp_source ${TEST_SOURCE_SOURCES})
