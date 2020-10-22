# sACN C++ Universe Discovery API unit tests

set(TEST_UNIVERSE_DISCOVERY_SOURCES
  test_cpp_universe_discovery.cpp
  main.cpp

  ${SACN_API_SOURCES}
  ${SACN_MOCK_SOURCES}
)

sacn_add_dynamic_test(test_cpp_universe_discovery ${TEST_UNIVERSE_DISCOVERY_SOURCES})
sacn_add_static_test(test_cpp_universe_discovery ${TEST_UNIVERSE_DISCOVERY_SOURCES})
