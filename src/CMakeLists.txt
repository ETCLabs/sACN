
set(SACN_LIB_TARGET_NAME sACN)

set(SACN_CONFIG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/config)
set(SACN_TARGETS_EXPORT_NAME ${PROJECT_NAME}Targets)

################################# sACN Library ################################

# Get the local source variables
include(sacn/sources.cmake)

add_library(${SACN_LIB_TARGET_NAME}
  ${SACN_PUBLIC_HEADERS}
  ${SACN_PRIVATE_HEADERS}
  ${SACN_SOURCES}
)
target_include_directories(${SACN_LIB_TARGET_NAME}
  PUBLIC ${SACN_INCLUDE}
  PRIVATE ${SACN_SRC}
)
if(DEFINED SACN_CONFIG_LOC)
  target_include_directories(${SACN_LIB_TARGET_NAME} PRIVATE ${SACN_CONFIG_LOC})
  target_compile_definitions(${SACN_LIB_TARGET_NAME} PRIVATE SACN_HAVE_CONFIG_H)
endif()
target_link_libraries(${SACN_LIB_TARGET_NAME} PUBLIC EtcPal)

# Organize sources in IDEs
source_group(TREE ${SACN_SRC}/sacn PREFIX src FILES
  ${SACN_PRIVATE_HEADERS}
  ${SACN_SOURCES}
)
source_group(TREE ${SACN_INCLUDE}/sacn PREFIX include FILES ${SACN_PUBLIC_HEADERS})

# Installation
install(TARGETS ${SACN_LIB_TARGET_NAME}
  EXPORT ${SACN_TARGETS_EXPORT_NAME}
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)
install(DIRECTORY ${SACN_ROOT}/include/sacn DESTINATION include FILES_MATCHING PATTERN "*.h")
