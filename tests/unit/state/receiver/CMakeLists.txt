# sACN internal receiver state unit tests

set(TEST_RECEIVER_STATE_SOURCES
  test_receiver_state.cpp
  main.cpp

  ${SACN_MEM_SOURCES}
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/util.c
  ${SACN_SRC}/sacn/receiver_state.c
  ${SACN_SRC}/sacn_mock/common.c
  ${SACN_SRC}/sacn_mock/source_loss.c
  ${SACN_SRC}/sacn_mock/sockets.c
  ${SACN_SRC}/sacn_mock/source_detector_state.c
)

sacn_add_dynamic_test(test_receiver_state ${TEST_RECEIVER_STATE_SOURCES})
sacn_add_static_test(test_receiver_state ${TEST_RECEIVER_STATE_SOURCES})
sacn_add_test(unit_test_receiver_state_pap_disabled_dynamic ${SACN_TEST}/configs/pap_disabled_dynamic ${TEST_RECEIVER_STATE_SOURCES})
sacn_add_test(unit_test_receiver_state_pap_disabled_static ${SACN_TEST}/configs/pap_disabled_static ${TEST_RECEIVER_STATE_SOURCES})
