# sACN C++ Merger API unit tests

set(TEST_MERGER_SOURCES
  test_cpp_merger.cpp
  main.cpp

  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/receiver.c
  ${SACN_SRC}/sacn/merge_receiver.c
  ${SACN_SRC}/sacn/source.c
  ${SACN_SRC}/sacn/util.c
  
  ${SACN_SRC}/sacn_mock/dmx_merger.c
  
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_cpp_merger ${SACN_TEST}/configs/dynamic ${TEST_MERGER_SOURCES})
sacn_add_static_test(test_cpp_merger ${SACN_TEST}/configs/static ${TEST_MERGER_SOURCES})
