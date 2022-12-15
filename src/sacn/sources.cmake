# sACN source manifest

set(SACN_PUBLIC_HEADERS
  ${SACN_INCLUDE}/sacn/common.h
  ${SACN_INCLUDE}/sacn/receiver.h
  ${SACN_INCLUDE}/sacn/merge_receiver.h
  ${SACN_INCLUDE}/sacn/source.h
  ${SACN_INCLUDE}/sacn/version.h
  ${SACN_INCLUDE}/sacn/dmx_merger.h
  ${SACN_INCLUDE}/sacn/source_detector.h
)
set(SACN_MEM_HEADERS
  ${SACN_SRC}/sacn/private/mem.h
  ${SACN_SRC}/sacn/private/mem/merge_receiver/merge_receiver.h
  ${SACN_SRC}/sacn/private/mem/merge_receiver/merge_receiver_source.h
  ${SACN_SRC}/sacn/private/mem/merge_receiver/merged_data.h
  ${SACN_SRC}/sacn/private/mem/receiver/receiver.h
  ${SACN_SRC}/sacn/private/mem/receiver/recv_thread_context.h
  ${SACN_SRC}/sacn/private/mem/receiver/remote_source.h
  ${SACN_SRC}/sacn/private/mem/receiver/sampling_ended.h
  ${SACN_SRC}/sacn/private/mem/receiver/sampling_period_netint.h
  ${SACN_SRC}/sacn/private/mem/receiver/sampling_started.h
  ${SACN_SRC}/sacn/private/mem/receiver/source_limit_exceeded.h
  ${SACN_SRC}/sacn/private/mem/receiver/source_pap_lost.h
  ${SACN_SRC}/sacn/private/mem/receiver/sources_lost.h
  ${SACN_SRC}/sacn/private/mem/receiver/status_lists.h
  ${SACN_SRC}/sacn/private/mem/receiver/to_erase.h
  ${SACN_SRC}/sacn/private/mem/receiver/tracked_source.h
  ${SACN_SRC}/sacn/private/mem/receiver/universe_data.h
  ${SACN_SRC}/sacn/private/mem/source/source.h
  ${SACN_SRC}/sacn/private/mem/source/source_netint.h
  ${SACN_SRC}/sacn/private/mem/source/source_universe.h
  ${SACN_SRC}/sacn/private/mem/source/unicast_destination.h
  ${SACN_SRC}/sacn/private/mem/source_detector/source_detector.h
  ${SACN_SRC}/sacn/private/mem/source_detector/source_detector_expired_source.h
  ${SACN_SRC}/sacn/private/mem/source_detector/universe_discovery_source.h
  ${SACN_SRC}/sacn/private/mem/common.h
)
set(SACN_PRIVATE_HEADERS
  ${SACN_MEM_HEADERS}
  ${SACN_SRC}/sacn/private/common.h
  ${SACN_SRC}/sacn/private/source_loss.h
  ${SACN_SRC}/sacn/private/dmx_merger.h
  ${SACN_SRC}/sacn/private/opts.h
  ${SACN_SRC}/sacn/private/pdu.h
  ${SACN_SRC}/sacn/private/receiver.h
  ${SACN_SRC}/sacn/private/merge_receiver.h
  ${SACN_SRC}/sacn/private/source.h
  ${SACN_SRC}/sacn/private/source_state.h
  ${SACN_SRC}/sacn/private/receiver_state.h
  ${SACN_SRC}/sacn/private/source_detector_state.h
  ${SACN_SRC}/sacn/private/util.h
  ${SACN_SRC}/sacn/private/source_detector.h
)
set(SACN_MEM_SOURCES
  ${SACN_SRC}/sacn/mem.c
  ${SACN_SRC}/sacn/mem/merge_receiver/merge_receiver.c
  ${SACN_SRC}/sacn/mem/merge_receiver/merge_receiver_source.c
  ${SACN_SRC}/sacn/mem/merge_receiver/merged_data.c
  ${SACN_SRC}/sacn/mem/receiver/receiver.c
  ${SACN_SRC}/sacn/mem/receiver/recv_thread_context.c
  ${SACN_SRC}/sacn/mem/receiver/remote_source.c
  ${SACN_SRC}/sacn/mem/receiver/sampling_ended.c
  ${SACN_SRC}/sacn/mem/receiver/sampling_period_netint.c
  ${SACN_SRC}/sacn/mem/receiver/sampling_started.c
  ${SACN_SRC}/sacn/mem/receiver/source_limit_exceeded.c
  ${SACN_SRC}/sacn/mem/receiver/source_pap_lost.c
  ${SACN_SRC}/sacn/mem/receiver/sources_lost.c
  ${SACN_SRC}/sacn/mem/receiver/status_lists.c
  ${SACN_SRC}/sacn/mem/receiver/to_erase.c
  ${SACN_SRC}/sacn/mem/receiver/tracked_source.c
  ${SACN_SRC}/sacn/mem/receiver/universe_data.c
  ${SACN_SRC}/sacn/mem/source/source.c
  ${SACN_SRC}/sacn/mem/source/source_netint.c
  ${SACN_SRC}/sacn/mem/source/source_universe.c
  ${SACN_SRC}/sacn/mem/source/unicast_destination.c
  ${SACN_SRC}/sacn/mem/source_detector/source_detector.c
  ${SACN_SRC}/sacn/mem/source_detector/source_detector_expired_source.c
  ${SACN_SRC}/sacn/mem/source_detector/universe_discovery_source.c
  ${SACN_SRC}/sacn/mem/common.c
)
set(SACN_API_SOURCES
  ${SACN_MEM_SOURCES}
  ${SACN_SRC}/sacn/dmx_merger.c
  ${SACN_SRC}/sacn/pdu.c
  ${SACN_SRC}/sacn/receiver.c
  ${SACN_SRC}/sacn/merge_receiver.c
  ${SACN_SRC}/sacn/source.c
  ${SACN_SRC}/sacn/util.c
  ${SACN_SRC}/sacn/source_detector.c
)

#TODO: There's probably a better name for this, but we'll
#find it when we add more serious testing.
set(SACN_MOCKABLE_SOURCES
  ${SACN_SRC}/sacn/common.c
  ${SACN_SRC}/sacn/source_loss.c
  ${SACN_SRC}/sacn/source_state.c
  ${SACN_SRC}/sacn/receiver_state.c
  ${SACN_SRC}/sacn/source_detector_state.c
  ${SACN_SRC}/sacn/sockets.c
)

set(SACN_SOURCES
  ${SACN_API_SOURCES}
  ${SACN_MOCKABLE_SOURCES}
)
