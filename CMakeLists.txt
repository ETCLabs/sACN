cmake_minimum_required(VERSION 3.15)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum macOS version targeted by sACN on macOS")
project(sACN)

set(SACN_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(SACN_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)
set(SACN_SRC ${CMAKE_CURRENT_LIST_DIR}/src)
set(SACN_CMAKE ${CMAKE_CURRENT_LIST_DIR}/cmake)

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
option(SACN_INSTALL_PDBS "Include PDBs in sACN install target" ON)

################################# Dependencies ################################

include(${SACN_CMAKE}/OssDependencyTools.cmake)
determine_compile_environment()
if(NOT COMPILING_AS_OSS)
  include(${SACN_CMAKE}/AddCMakeTools.cmake)
endif()
include(${SACN_CMAKE}/ResolveDependencies.cmake)

############################## Standalone support #############################

if(NOT COMPILING_AS_OSS)
  include(${CMAKE_TOOLS_MODULES}/QualityGateHelpers.cmake)

  setup_standalone_compile()
  setup_clang_format()
  # TODO: setup_clang_tidy()
  setup_address_sanitizer()
endif()

################################ Main libraries ###############################

add_subdirectory(src)

#################################### Tests ####################################

if(SACN_BUILD_TESTS)
  include(GoogleTest)
  enable_testing()
  add_subdirectory(external/fff)
  add_subdirectory(tests)
endif()

################################### Examples ################################## 

if(SACN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
