#include "sacn_config_common.h"

#define SACN_DYNAMIC_MEM 0

#define SACN_RECEIVER_MAX_UNIVERSES            30
#define SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE 8
#define SACN_MAX_NETINTS                       100

// The OS limits multicast memberships per socket (Linux: net.ipv4.igmp_max_memberships, default 20), and each
// subscription consumes one membership per multicast-capable network interface. Some CI/sandbox runners expose many
// interfaces (e.g. eth0 + docker0 + services1 + lo == 4), so subs_per_socket must stay low enough that
// subs_per_socket * num_interfaces <= the OS limit. 5 keeps us within 20 even with 4 interfaces.
#define SACN_RECEIVER_MAX_SUBS_PER_SOCKET 5

// The default 32 KB receive buffer overflows on CI/sandbox runners that expose many multicast-capable interfaces:
// each source packet is looped back once per interface into userspace before the library de-dups it, so an
// at-scale burst (many universes x interfaces x sources) far exceeds 32 KB and the kernel drops packets
// (visible as net/snmp Udp RcvbufErrors), starving some universes. Raise toward the OS rmem_max (here 212992).
#define SACN_RECEIVER_SOCKET_RCVBUF_SIZE 212992

#define SACN_SOURCE_MAX_SOURCES              10
#define SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE 2048

#define SACN_SOURCE_DETECTOR_MAX_SOURCES              2
#define SACN_SOURCE_DETECTOR_MAX_UNIVERSES_PER_SOURCE 700

#define SACN_DMX_MERGER_MAX_SLOTS 500

#undef SACN_LOGGING_ENABLED
#define SACN_LOGGING_ENABLED 1
