# sACN source disabled configuration unit tests

set(TEST_SOURCE_DISABLED_SOURCES
  test_source_disabled.cpp
  main.cpp

  ${SACN_SOURCES}
)

sacn_add_test(unit_test_source_disabled_dynamic ${SACN_TEST}/configs/source_disabled_dynamic ${TEST_SOURCE_DISABLED_SOURCES})
sacn_add_test(unit_test_source_disabled_static ${SACN_TEST}/configs/source_disabled_static ${TEST_SOURCE_DISABLED_SOURCES})
