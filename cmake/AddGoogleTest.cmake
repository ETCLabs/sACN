# Sets up GoogleTest when added as a git submodule.

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS ${SACN_ROOT}/.git)
  # Update the submodules to bring in googletest
  execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  RESULT_VARIABLE GIT_SUBMOD_RESULT)
  if(NOT GIT_SUBMOD_RESULT EQUAL "0")
    message(FATAL_ERROR "Error updating the GoogleTest submodule (code ${GIT_SUBMOD_RESULT}), try checking out the submodule manually?")
  endif()
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

add_subdirectory(external/googletest)

mark_as_advanced(
  gmock_build_tests
  gtest_build_samples
  gtest_build_tests
  gtest_disable_pthreads
  gtest_force_shared_crt
  gtest_hide_internal_symbols
  BUILD_GMOCK
  BUILD_GTEST
)

set_target_properties(gtest gtest_main gmock gmock_main
  PROPERTIES FOLDER "tests"
)
