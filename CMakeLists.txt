cmake_minimum_required(VERSION 3.10)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum macOS version targeted by sACN on macOS")
project(sACN VERSION 2.0.0)

# Sometimes the seems to be some weirdness with drive letter capitalization on Windows, so do a
# case-insensitive comparision
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL Windows)
  string(TOLOWER ${PROJECT_SOURCE_DIR} PROJECT_SOURCE_DIR_LC)
  string(TOLOWER ${CMAKE_SOURCE_DIR} CMAKE_SOURCE_DIR_LC)
else()
  set(PROJECT_SOURCE_DIR_LC ${PROJECT_SOURCE_DIR})
  set(CMAKE_SOURCE_DIR_LC ${CMAKE_SOURCE_DIR})
endif()

if(PROJECT_SOURCE_DIR_LC STREQUAL CMAKE_SOURCE_DIR_LC)
  if (MSVC)
    add_compile_options(/W4 /permissive-)
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      add_compile_options(-Wno-sign-conversion)
    else()
      add_compile_options(-Wno-sign-compare)
    endif()
  endif()

  # Allow folder grouping in generated IDE projects
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  # Add a target to run Clang Format
  file(GLOB_RECURSE FILES_TO_REFORMAT 
    examples/*.c
    examples/*.cpp
    examples/*.h
    include/*.h
    src/*.c
    src/*.cpp
    src/*.h
    tests/*.c
    tests/*.cpp
    tests/*.h
  )
  add_custom_target(reformat_all COMMAND clang-format -i ${FILES_TO_REFORMAT})
  set_target_properties(reformat_all PROPERTIES FOLDER tools)
endif()

set(SACN_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(SACN_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)
set(SACN_SRC ${CMAKE_CURRENT_LIST_DIR}/src)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

###################### Compile Options and Configuration ######################

if(DEFINED SACN_CONFIG_LOC)
  get_filename_component(SACN_CONFIG_LOC ${SACN_CONFIG_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
endif()

option(SACN_BUILD_TESTS "Build the sACN unit tests" OFF)
option(SACN_BUILD_EXAMPLES "Build the sACN example applications" OFF)
option(SACN_BUILD_TEST_TOOLS "Build the sACN test tools (typically used in development only)" OFF)

################################ Main libraries ###############################

add_subdirectory(src)

#################################### Tests ####################################

if(SACN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(external/fff)
  include(AddGoogleTest)
  add_subdirectory(tests)
endif()

################################### Examples ################################## 

if(SACN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
