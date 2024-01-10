# sACN C Merger API unit tests

set(TEST_MERGER_SOURCES
  test_dmx_merger.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_merger ${TEST_MERGER_SOURCES})
sacn_add_static_test(test_merger ${TEST_MERGER_SOURCES})
sacn_add_test(unit_test_merger_internal_buffers_disabled_dynamic ${SACN_TEST}/configs/merger_internal_buffers_disabled_dynamic ${TEST_MERGER_SOURCES})
sacn_add_test(unit_test_merger_internal_buffers_disabled_static ${SACN_TEST}/configs/merger_internal_buffers_disabled_static ${TEST_MERGER_SOURCES})
