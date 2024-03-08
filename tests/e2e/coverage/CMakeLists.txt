# sACN end-to-end tests that execute as much library code as possible for the sanitizers

set(COVERAGE_TEST_SOURCES
  coverage_test.cpp
  main.cpp
)

sacn_add_dynamic_test(coverage_test ${COVERAGE_TEST_SOURCES})
sacn_add_static_test(coverage_test ${COVERAGE_TEST_SOURCES})
