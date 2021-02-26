# sACN source disabled configuration unit tests

set(TEST_SOURCE_DISABLED_DIR ${CMAKE_CURRENT_LIST_DIR})

set(TEST_SOURCE_DISABLED_SOURCES
  test_source_disabled.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_source_disabled ${TEST_SOURCE_DISABLED_DIR}/config/dynamic ${TEST_SOURCE_DISABLED_SOURCES})
sacn_add_static_test(test_source_disabled ${TEST_SOURCE_DISABLED_DIR}/config/static ${TEST_SOURCE_DISABLED_SOURCES})
