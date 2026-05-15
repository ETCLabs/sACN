#include "sacn_config_common.h"

#define SACN_DYNAMIC_MEM 1

// Tests indicate that the Linux runner only supports up to 10 subscriptions per socket.
#define SACN_RECEIVER_MAX_SUBS_PER_SOCKET 10

#define SACN_DMX_MERGER_MAX_SLOTS 500

#undef SACN_LOGGING_ENABLED
#define SACN_LOGGING_ENABLED 1
