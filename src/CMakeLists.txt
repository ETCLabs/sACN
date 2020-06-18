
set(SACN_LIB_TARGET_NAME sACN)

set(SACN_CONFIG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/config)
set(SACN_TARGETS_EXPORT_NAME ${PROJECT_NAME}Targets)

################################# Dependencies ################################

include(SacnAddDependency)

if(SACN_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
endif()
sacn_add_dependency(EtcPal ETCPAL_DIR)

if(TARGET EtcPalMock)
  set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)
endif()

################################# sACN Library ################################

set(SACN_PUBLIC_HEADERS
  ${SACN_INCLUDE}/sacn/common.h
  ${SACN_INCLUDE}/sacn/receiver.h
  ${SACN_INCLUDE}/sacn/easy_receiver.h
  ${SACN_INCLUDE}/sacn/source.h
  ${SACN_INCLUDE}/sacn/version.h
  ${SACN_INCLUDE}/sacn/dmx_merger.h
)
set(SACN_PRIVATE_HEADERS
  ${SACN_SRC}/sacn/private/common.h
  ${SACN_SRC}/sacn/private/data_loss.h
  ${SACN_SRC}/sacn/private/mem.h
  ${SACN_SRC}/sacn/private/dmx_merger.h
  ${SACN_SRC}/sacn/private/opts.h
  ${SACN_SRC}/sacn/private/pdu.h
  ${SACN_SRC}/sacn/private/receiver.h
  ${SACN_SRC}/sacn/private/easy_receiver.h
  ${SACN_SRC}/sacn/private/source.h
  ${SACN_SRC}/sacn/private/util.h
)
set(SACN_SOURCES
  ${SACN_SRC}/sacn/common.c
  ${SACN_SRC}/sacn/data_loss.c
  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/dmx_merger.c
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/receiver.c
  ${SACN_SRC}/sacn/easy_receiver.c
  ${SACN_SRC}/sacn/sockets.c
  ${SACN_SRC}/sacn/source.c
  ${SACN_SRC}/sacn/util.c
)

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
