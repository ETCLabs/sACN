set(SACN_TEST ${CMAKE_CURRENT_LIST_DIR})

include(SacnSourceManifest)
add_subdirectory(unit)
add_subdirectory(integration)
