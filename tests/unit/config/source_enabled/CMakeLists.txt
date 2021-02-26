# sACN source enabled configuration unit tests

set(TEST_SOURCE_ENABLED_DIR ${CMAKE_CURRENT_LIST_DIR})

set(TEST_SOURCE_ENABLED_SOURCES
  test_source_enabled.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_source_enabled ${TEST_SOURCE_ENABLED_DIR}/config/dynamic ${TEST_SOURCE_ENABLED_SOURCES})
sacn_add_static_test(test_source_enabled ${TEST_SOURCE_ENABLED_DIR}/config/static ${TEST_SOURCE_ENABLED_SOURCES})
