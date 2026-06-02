if(SACN_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
endif()

if(COMPILING_AS_OSS)
  if(NOT SACN_SANDBOX_MODE)
    get_cpm()

    add_oss_dependency(EtcPal GIT_REPOSITORY https://github.com/ETCLabs/EtcPal.git)

    if(SACN_BUILD_TESTS)
      add_oss_dependency(googletest GIT_REPOSITORY https://github.com/google/googletest.git)
      add_oss_dependency(GSL GIT_REPOSITORY https://github.com/microsoft/gsl.git)
    endif()
  endif()
else()
  include(${CMAKE_TOOLS_MODULES}/DependencyManagement.cmake)
  
  if(SACN_SANDBOX_MODE)
    # Prevent the download by pointing to previously-downloaded dependencies.
    add_project_dependency(EtcPal SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/etcpal-src)

    if(SACN_BUILD_TESTS)
      add_project_dependency(googletest SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/googletest-src)
      add_project_dependency(GSL SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/gsl-src)
    endif()
  else()
    add_project_dependencies()

    if(SACN_BUILD_TESTS)
      add_project_dependency(googletest)
      add_project_dependency(GSL)
    endif()
  endif()
endif()

if(TARGET EtcPalMock)
  set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)
endif()
