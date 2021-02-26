# sACN C Source Detector API unit tests

set(TEST_SOURCE_DETECTOR_SOURCES
  test_source_detector.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_source_detector ${SACN_TEST}/configs/dynamic ${TEST_SOURCE_DETECTOR_SOURCES})
sacn_add_static_test(test_source_detector ${SACN_TEST}/configs/static ${TEST_SOURCE_DETECTOR_SOURCES})
