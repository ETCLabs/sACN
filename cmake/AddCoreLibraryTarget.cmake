
include(${SACN_CMAKE}/SacnSourceManifest.cmake)

function(sacn_add_core_library_target target_name)
  set (extra_args ${ARGN})
  list(LENGTH extra_args extra_count)
  if(${extra_count} GREATER 0)
    list(GET extra_args 0 config_dir)
  endif()
  if(${extra_count} GREATER 1)
    list(GET extra_args 1 ide_folder)
  endif()

  add_library(${target_name}
    ${SACN_PUBLIC_HEADERS}
    ${SACN_PRIVATE_HEADERS}
    ${SACN_SOURCES}
  )
  target_include_directories(${target_name}
    PUBLIC ${SACN_INCLUDE}
    PRIVATE ${SACN_SRC}
  )

  if(DEFINED config_dir)
    target_include_directories(${target_name} PRIVATE ${config_dir})
    target_compile_definitions(${target_name} PRIVATE SACN_HAVE_CONFIG_H)
  endif()

  target_link_libraries(${target_name} PUBLIC EtcPal)

  # Organize sources in IDEs
  source_group(TREE ${SACN_SRC}/sacn PREFIX src FILES
    ${SACN_PRIVATE_HEADERS}
    ${SACN_SOURCES}
  )
  source_group(TREE ${SACN_INCLUDE}/sacn PREFIX include FILES ${SACN_PUBLIC_HEADERS})

  if(DEFINED ide_folder)
    set_target_properties(${target_name} PROPERTIES FOLDER ${ide_folder})
  endif()
endfunction()
