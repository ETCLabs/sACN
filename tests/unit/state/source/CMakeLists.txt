# sACN internal source state unit tests

set(TEST_SOURCE_STATE_SOURCES
  test_source_state.cpp
  main.cpp

  ${SACN_MEM_SOURCES}
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/util.c
  ${SACN_SRC}/sacn/source_state.c
  ${SACN_SRC}/sacn_mock/common.c
  ${SACN_SRC}/sacn_mock/source_loss.c
  ${SACN_SRC}/sacn_mock/sockets.c
)

sacn_add_dynamic_test(test_source_state ${TEST_SOURCE_STATE_SOURCES})
sacn_add_static_test(test_source_state ${TEST_SOURCE_STATE_SOURCES})
