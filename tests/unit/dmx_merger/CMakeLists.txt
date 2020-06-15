# sACN DMX Merger unit tests

set(TEST_DMX_MERGER_SOURCES
  test_dmx_merger.cpp
  main.cpp

  ${SACN_SRC}/sacn/dmx_merger.c
  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/util.c

  ${SACN_SRC}/sacn_mock/data_loss.c
  ${SACN_SRC}/sacn_mock/common.c
  ${SACN_SRC}/sacn_mock/sockets.c
)

sacn_add_dynamic_test(test_dmx_merger ${TEST_DMX_MERGER_SOURCES})
sacn_add_static_test(test_dmx_merger ${TEST_DMX_MERGER_SOURCES})
