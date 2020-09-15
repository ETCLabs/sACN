# sACN C Merger API unit tests

set(TEST_MERGER_SOURCES
  test_dmx_merger.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_merger ${TEST_MERGER_SOURCES})
sacn_add_static_test(test_merger ${TEST_MERGER_SOURCES})
