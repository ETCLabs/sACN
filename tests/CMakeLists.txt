set(SACN_TEST ${CMAKE_CURRENT_LIST_DIR})
set(TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

include(${SACN_CMAKE}/SacnSourceManifest.cmake)

function(sacn_test_setup target_name config_dir)
  add_executable(${target_name} ${ARGN})

  target_compile_definitions(${target_name} PRIVATE SACN_HAVE_CONFIG_H)
  target_include_directories(${target_name} PRIVATE
    ${config_dir}
    ${SACN_TEST}/configs
    ${SACN_TEST}
    ${SACN_INCLUDE}
    ${SACN_SRC}
  )
  if(NOT COMPILING_AS_OSS)
    target_clang_tidy(${target_name})
  endif()
  set_target_properties(${target_name} PROPERTIES CXX_STANDARD 20 FOLDER tests)
  gtest_discover_tests(${target_name} EXTRA_ARGS "--gtest_output=xml:${TEST_BIN_DIR}/test-results/")
endfunction()

function(sacn_add_test test_name config_dir)
  set(target_name ${test_name})
  sacn_test_setup(${target_name} ${config_dir} ${ARGN})
  target_link_libraries(${target_name} PRIVATE EtcPalMock gtest gmock meekrosoft::fff GSL)
endfunction()

add_subdirectory(unit)
add_subdirectory(integration)
if(SACN_ENABLE_E2E_TESTS)
  add_subdirectory(e2e)
endif()
