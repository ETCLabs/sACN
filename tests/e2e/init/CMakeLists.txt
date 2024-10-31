# sACN end-to-end tests for initialization

set(INIT_TEST_SOURCES
  init_test.cpp
  main.cpp
)

sacn_add_dynamic_test(init_test ${INIT_TEST_SOURCES})
sacn_add_static_test(init_test ${INIT_TEST_SOURCES})
