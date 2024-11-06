set(E2E_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})
set(E2E_TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

function(sacn_add_e2e_test test_name config_dir)
  set(target_name ${test_name})
  sacn_test_setup(${target_name} ${config_dir} ${ARGN})
  target_link_libraries(${target_name} PRIVATE gtest gmock GSL)
endfunction()

include(${SACN_CMAKE}/AddCoreLibraryTarget.cmake)
sacn_add_core_library_target(sacn_library_dynamic ${SACN_TEST}/configs/dynamic tests)
sacn_add_core_library_target(sacn_library_static ${SACN_TEST}/configs/static tests)

target_include_directories(sacn_library_dynamic PRIVATE ${SACN_TEST}/configs)
target_include_directories(sacn_library_static PRIVATE ${SACN_TEST}/configs)

function(sacn_add_dynamic_test test_name)
  set(target_name e2e_${test_name}_dynamic)
  sacn_add_e2e_test(${target_name} ${SACN_TEST}/configs/dynamic ${ARGN})
  target_link_libraries(${target_name} PRIVATE sacn_library_dynamic)
endfunction()

function(sacn_add_static_test test_name)
  set(target_name e2e_${test_name}_static)
  sacn_add_e2e_test(${target_name} ${SACN_TEST}/configs/static ${ARGN})
  target_link_libraries(${target_name} PRIVATE sacn_library_static)
endfunction()

add_subdirectory(coverage)
add_subdirectory(init)
