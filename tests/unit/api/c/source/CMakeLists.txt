# sACN C Source API unit tests

set(TEST_SOURCE_SOURCES
  test_source.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_source ${TEST_SOURCE_SOURCES})
sacn_add_static_test(test_source ${TEST_SOURCE_SOURCES})
