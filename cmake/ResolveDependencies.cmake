if(SACN_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
endif()

if(COMPILING_AS_OSS)
  get_cpm()

  add_oss_dependency(EtcPal GIT_REPOSITORY https://github.com/ETCLabs/EtcPal.git)

  if(SACN_BUILD_TESTS)
    add_oss_dependency(googletest GIT_REPOSITORY https://github.com/google/googletest.git)
    add_oss_dependency(GSL GIT_REPOSITORY https://github.com/microsoft/gsl.git)
  endif()
else()
  include(${CMAKE_TOOLS_MODULES}/DependencyManagement.cmake)
  add_project_dependencies()

  if(SACN_BUILD_TESTS)
    add_project_dependency(googletest)
    add_project_dependency(GSL)
  endif()
endif()

if(TARGET EtcPalMock)
  set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)
endif()
