# sACN source detector disabled configuration integration tests

set(TEST_SOURCE_DETECTOR_DISABLED_SOURCES
  test_source_detector_disabled.cpp
  main.cpp

  ${SACN_SOURCES}
)

sacn_add_test(integration_test_detector_disabled_dynamic ${SACN_TEST}/configs/source_detector_disabled_dynamic ${TEST_SOURCE_DETECTOR_DISABLED_SOURCES})
sacn_add_test(integration_test_detector_disabled_static ${SACN_TEST}/configs/source_detector_disabled_static ${TEST_SOURCE_DETECTOR_DISABLED_SOURCES})
