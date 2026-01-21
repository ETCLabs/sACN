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
option(SACN_ENABLE_E2E_TESTS "Enable the end-to-end sACN tests (requires SACN_BUILD_TESTS)" OFF)
option(SACN_BUILD_EXAMPLES "Build the sACN example applications" OFF)
option(SACN_BUILD_TEST_TOOLS "Build the sACN test tools (typically used in development only)" OFF)
option(SACN_INSTALL_PDBS "Include PDBs in sACN install target" ON)

set(SACN_SRTP_CRYPTO_POLICY_VALUES "default;aes-cm-128-hmac-sha1-32;aes-cm-128-null-auth;null-cipher-hmac-sha1-80;null-cipher-hmac-null;aes-cm-256-hmac-sha1-80;aes-cm-256-hmac-sha1-32;aes-cm-256-null-auth;aes-cm-192-hmac-sha1-80;aes-cm-192-hmac-sha1-32;aes-cm-192-null-auth;aes-gcm-128-16-auth;aes-gcm-256-16-auth")
set(SACN_SRTP_CRYPTO_POLICY "aes-gcm-256-16-auth" CACHE STRING "SRTP crypto policy, can be one of: ${SACN_SRTP_CRYPTO_POLICY_VALUES}")
set_property(CACHE SACN_SRTP_CRYPTO_POLICY PROPERTY STRINGS ${SACN_SRTP_CRYPTO_POLICY_VALUES})
if(NOT SACN_SRTP_CRYPTO_POLICY IN_LIST SACN_SRTP_CRYPTO_POLICY_VALUES)
  message(FATAL_ERROR "Invalid value for SACN_SRTP_CRYPTO_POLICY: ${SACN_SRTP_CRYPTO_POLICY}. Allowed values are: ${SACN_SRTP_CRYPTO_POLICY_VALUES}")
endif()
message(STATUS "Using SRTP crypto policy: ${SACN_SRTP_CRYPTO_POLICY}")

option(SACN_ENABLE_SRTP_REKEY_TEST "Enable the SRTP rekey test" OFF)

####################### Dependencies & standalone support ######################

include(${SACN_CMAKE}/OssDependencyTools.cmake)
determine_compile_environment()

if(NOT COMPILING_AS_OSS)
  include(${SACN_CMAKE}/AddCMakeTools.cmake)
  include(${CMAKE_TOOLS_MODULES}/QualityGateHelpers.cmake)
  
  # While the functions below already check for standalone, this check is performed to avoid the risk of unknown
  # function errors during CMake generation (i.e. when the application's CMake Tools is out-of-date).
  get_directory_property(IS_DEPENDENCY PARENT_DIRECTORY)
  if(NOT IS_DEPENDENCY)
    setup_clang_format(${SACN_ROOT})
    setup_clang_tidy()
    setup_address_sanitizer()
    setup_memory_sanitizer()
    setup_undefined_behavior_sanitizer()
    setup_thread_sanitizer()
  endif()
endif()

# Dependency configuration overrides
set(MBEDTLS_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/_deps/mbedtls-src/include CACHE PATH "Override" FORCE)
set(MbedTLS_FOUND TRUE CACHE BOOL "" FORCE)
set(MBEDTLS_LIBRARY mbedtls CACHE STRING "" FORCE)
set(MBEDX509_LIBRARY mbedx509 CACHE STRING "" FORCE)
set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "" FORCE)
set(CRYPTO_LIBRARY "mbedtls" CACHE STRING "Override" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Override" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Override" FORCE)
set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "Override" FORCE)
set(LIBSRTP_TEST_APPS OFF CACHE BOOL "Override" FORCE)

include(${SACN_CMAKE}/ResolveDependencies.cmake)

# Set up standalone compile settings after dependencies (since they trigger warnings as errors)
if(NOT COMPILING_AS_OSS)
  get_directory_property(IS_DEPENDENCY PARENT_DIRECTORY)
  if(NOT IS_DEPENDENCY)
    setup_standalone_compile()
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      add_compile_options(-Wno-deprecated-declarations)  # For compatibility with all platforms
    endif()
  endif()
endif()

################################ Main libraries ###############################

# Get the local source variables

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
