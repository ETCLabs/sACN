include(GoogleTest)

function(sacn_add_dynamic_test test_name)
  set(target_name ${test_name}_dynamic)
  add_executable(${target_name} ${ARGN})

  target_compile_definitions(${target_name} PRIVATE SACN_HAVE_CONFIG_H)
  target_include_directories(${target_name} PRIVATE
    ${SACN_TEST}/configs/dynamic
    ${SACN_INCLUDE}
    ${SACN_SRC}
  )
  target_link_libraries(${target_name} PRIVATE EtcPalMock gtest meekrosoft::fff)
  set_target_properties(${target_name} PROPERTIES CXX_STANDARD 17 FOLDER tests)
  gtest_discover_tests(${target_name})
endfunction()

function(sacn_add_static_test test_name)
  set(target_name ${test_name}_static)
  add_executable(${target_name} ${ARGN})

  target_compile_definitions(${target_name} PRIVATE SACN_HAVE_CONFIG_H)
  target_include_directories(${target_name} PRIVATE
    ${SACN_TEST}/configs/static
    ${SACN_INCLUDE}
    ${SACN_SRC}
  )
  target_link_libraries(${target_name} PRIVATE EtcPalMock gtest meekrosoft::fff)
  set_target_properties(${target_name} PROPERTIES CXX_STANDARD 17 FOLDER tests)
  gtest_discover_tests(${target_name})
endfunction()

add_subdirectory(utils)
add_subdirectory(receiver)
add_subdirectory(dmx_merger)
