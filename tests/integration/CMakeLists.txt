include(GoogleTest)

set(INTEGRATION_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(INTEGRATION_TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

function(sacn_add_test test_name config_dir)
  set(target_name integration_${test_name})
  add_executable(${target_name} ${ARGN})

  target_compile_definitions(${target_name} PRIVATE SACN_HAVE_CONFIG_H)
  target_include_directories(${target_name} PRIVATE
    ${config_dir}
    ${SACN_INCLUDE}
    ${SACN_SRC}
  )
  target_link_libraries(${target_name} PRIVATE EtcPalMock gtest meekrosoft::fff)
  set_target_properties(${target_name} PROPERTIES CXX_STANDARD 17 FOLDER tests)
  gtest_discover_tests(${target_name} EXTRA_ARGS "--gtest_output=xml:${INTEGRATION_TEST_BIN_DIR}/test-results/")
endfunction()

function(sacn_add_dynamic_test test_name)
  sacn_add_test(${test_name}_dynamic ${SACN_TEST}/configs/dynamic ${ARGN})
endfunction()

function(sacn_add_static_test test_name)
  sacn_add_test(${test_name}_static ${SACN_TEST}/configs/static ${ARGN})
endfunction()

add_subdirectory(api)
