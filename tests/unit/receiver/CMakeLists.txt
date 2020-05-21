# sACN Receiver unit tests

set(TEST_RECEIVER_SOURCES
  test_receiver.cpp
  test_receiver_with_network.cpp
  main.cpp

  ${SACN_SRC}/sacn/receiver.c
  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/util.c

  ${SACN_SRC}/sacn_mock/data_loss.c
  ${SACN_SRC}/sacn_mock/common.c
  ${SACN_SRC}/sacn_mock/sockets.c
)

sacn_add_dynamic_test(test_receiver ${TEST_RECEIVER_SOURCES})
sacn_add_static_test(test_receiver ${TEST_RECEIVER_SOURCES})
