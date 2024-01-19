set(E2E_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(E2E_TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

function(sacn_add_e2e_test test_name config_dir)
  set(target_name ${test_name})
  sacn_test_setup(${target_name} ${config_dir} ${ARGN})
  target_link_libraries(${target_name} PRIVATE sACN gtest gmock)
endfunction()

function(sacn_add_dynamic_test test_name)
  sacn_add_e2e_test(e2e_${test_name}_dynamic ${SACN_TEST}/configs/dynamic ${ARGN})
endfunction()

function(sacn_add_static_test test_name)
  sacn_add_e2e_test(e2e_${test_name}_static ${SACN_TEST}/configs/static ${ARGN})
endfunction()

add_subdirectory(coverage)
