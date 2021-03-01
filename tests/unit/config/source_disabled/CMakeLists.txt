# sACN source disabled configuration unit tests

set(TEST_SOURCE_DISABLED_SOURCES
  test_source_disabled.cpp
  main.cpp

  ${SACN_SOURCES}
)

sacn_add_dynamic_test(test_source_disabled ${SACN_TEST}/configs/source_disabled_dynamic ${TEST_SOURCE_DISABLED_SOURCES})
sacn_add_static_test(test_source_disabled ${SACN_TEST}/configs/source_disabled_static ${TEST_SOURCE_DISABLED_SOURCES})
