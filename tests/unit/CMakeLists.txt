include(GoogleTest)

set(UNIT_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(UNIT_TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

function(sacn_add_dynamic_test test_name)
  sacn_add_test(unit_${test_name}_dynamic ${SACN_TEST}/configs/dynamic ${ARGN})
endfunction()

function(sacn_add_static_test test_name)
  sacn_add_test(unit_${test_name}_static ${SACN_TEST}/configs/static ${ARGN})
endfunction()

add_subdirectory(utils)
add_subdirectory(api)
add_subdirectory(state)
add_subdirectory(config)
