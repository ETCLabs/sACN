# sACN C++ Source API unit tests

set(TEST_SOURCE_SOURCES
  test_cpp_source.cpp
  main.cpp

  ${SACN_MEM_SOURCES}
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/util.c
  
  ${SACN_SRC}/sacn_mock/source.c
  
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_cpp_source ${TEST_SOURCE_SOURCES})
sacn_add_static_test(test_cpp_source ${TEST_SOURCE_SOURCES})
