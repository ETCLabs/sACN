set(SACN_TEST ${CMAKE_CURRENT_LIST_DIR})
set(TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

include(${SACN_CMAKE}/SacnSourceManifest.cmake)

function(sacn_add_test test_name config_dir)
  set(target_name ${test_name})
  add_executable(${target_name} ${ARGN})

  target_compile_definitions(${target_name} PRIVATE SACN_HAVE_CONFIG_H)
  target_include_directories(${target_name} PRIVATE
    ${config_dir}
    ${SACN_INCLUDE}
    ${SACN_SRC}
  )
  target_link_libraries(${target_name} PRIVATE EtcPalMock gtest gmock meekrosoft::fff)
  set_target_properties(${target_name} PROPERTIES CXX_STANDARD 20 FOLDER tests)
  gtest_discover_tests(${target_name} EXTRA_ARGS "--gtest_output=xml:${TEST_BIN_DIR}/test-results/")
endfunction()

add_subdirectory(unit)
add_subdirectory(integration)
