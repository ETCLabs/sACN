# sACN C++ Merger API unit tests

list(REMOVE_ITEM SACN_API_SOURCES ${SACN_SRC}/sacn/dmx_merger.c)
list(APPEND SACN_MOCK_SOURCES ${SACN_SRC}/sacn_mock/dmx_merger.c)

set(TEST_MERGER_SOURCES
  test_cpp_merger.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_cpp_merger ${TEST_MERGER_SOURCES})
sacn_add_static_test(test_cpp_merger ${TEST_MERGER_SOURCES})

target_compile_definitions(test_cpp_merger_dynamic PRIVATE TESTING_CPP_MERGER)
target_compile_definitions(test_cpp_merger_static PRIVATE TESTING_CPP_MERGER)
