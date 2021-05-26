# sACN DMX merger disabled configuration integration tests

set(TEST_DMX_MERGER_DISABLED_SOURCES
  test_dmx_merger_disabled.cpp
  main.cpp

  ${SACN_SOURCES}
)

sacn_add_test(integration_test_dmx_merger_disabled_dynamic ${SACN_TEST}/configs/dmx_merger_disabled_dynamic ${TEST_DMX_MERGER_DISABLED_SOURCES})
sacn_add_test(integration_test_dmx_merger_disabled_static ${SACN_TEST}/configs/dmx_merger_disabled_static ${TEST_DMX_MERGER_DISABLED_SOURCES})
