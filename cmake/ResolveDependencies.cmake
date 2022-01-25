if(SACN_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
endif()

if(COMPILING_AS_OSS)
  get_cpm()

  get_dependency_version(EtcPal)
  CPMAddPackage(
    NAME EtcPal
    VERSION ${ETCPAL_VERSION}
    GIT_REPOSITORY https://github.com/ETCLabs/EtcPal.git
    GIT_TAG ${ETCPAL_GIT_TAG}
  )

  if(SACN_BUILD_TESTS)
    get_dependency_version(googletest)
    CPMAddPackage(
      NAME googletest
      VERSION ${GOOGLETEST_VERSION}
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG ${GOOGLETEST_GIT_TAG}
    )
  endif()
else()
  include(${CMAKE_TOOLS_MODULES}/DependencyManagement.cmake)
  add_project_dependencies()

  if(SACN_BUILD_TESTS)
    add_project_dependency(googletest)
  endif()
endif()

if(TARGET EtcPalMock)
  set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)
endif()
