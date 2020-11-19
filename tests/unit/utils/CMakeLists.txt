# sACN source loss behavior unit tests

set(TEST_UTILS_SOURCES
  test_source_loss.cpp
  test_mem.cpp
  test_sockets.cpp
  main.cpp

  ${SACN_SRC}/sacn/source_loss.c
  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/sockets.c
)

sacn_add_dynamic_test(test_utils ${TEST_UTILS_SOURCES})
sacn_add_static_test(test_utils ${TEST_UTILS_SOURCES})
