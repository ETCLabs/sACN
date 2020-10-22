# sACN source manifest

set(SACN_PUBLIC_HEADERS
  ${SACN_INCLUDE}/sacn/common.h
  ${SACN_INCLUDE}/sacn/receiver.h
  ${SACN_INCLUDE}/sacn/merge_receiver.h
  ${SACN_INCLUDE}/sacn/source.h
  ${SACN_INCLUDE}/sacn/version.h
  ${SACN_INCLUDE}/sacn/dmx_merger.h
  ${SACN_INCLUDE}/sacn/universe_discovery.h
)
set(SACN_PRIVATE_HEADERS
  ${SACN_SRC}/sacn/private/common.h
  ${SACN_SRC}/sacn/private/data_loss.h
  ${SACN_SRC}/sacn/private/mem.h
  ${SACN_SRC}/sacn/private/dmx_merger.h
  ${SACN_SRC}/sacn/private/opts.h
  ${SACN_SRC}/sacn/private/pdu.h
  ${SACN_SRC}/sacn/private/receiver.h
  ${SACN_SRC}/sacn/private/merge_receiver.h
  ${SACN_SRC}/sacn/private/source.h
  ${SACN_SRC}/sacn/private/util.h
  ${SACN_SRC}/sacn/private/universe_discovery.h
)
set(SACN_API_SOURCES
  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/dmx_merger.c
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/receiver.c
  ${SACN_SRC}/sacn/merge_receiver.c
  ${SACN_SRC}/sacn/source.c
  ${SACN_SRC}/sacn/util.c
  ${SACN_SRC}/sacn/universe_discovery.c
)

#TODO: There's probably a better name for this, but we'll
#find it when we add more serious testing.
set(SACN_MOCKABLE_SOURCES
  ${SACN_SRC}/sacn/common.c
  ${SACN_SRC}/sacn/data_loss.c
  ${SACN_SRC}/sacn/sockets.c
)

set(SACN_SOURCES
  ${SACN_API_SOURCES}
  ${SACN_MOCKABLE_SOURCES}
)
