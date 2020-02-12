# Using the sACN Receiver API                                                     {#using_receiver}

The sACN Receiver API provides a method for applications to receive and process sACN data on one or
more universes. Currently only NULL START code data (DMX) and ETC's Per-Channel Priority extension
are processed by the receiver API.

## Initialization and Destruction

The sACN library has overall init and deinit functions that should be called once each at
application startup and shutdown time. These functions interface with the EtcPal \ref etcpal_log
API to configure what happens when the sACN library logs messages. Optionally pass an
EtcPalLogParams structure to use this functionality. This structure can be shared across different
ETC library modules.

```c
#include "sacn/receiver.h"

// Called at startup
void startup_sacn(void)
{
  EtcPalLogParams log_params;
  // Initialize log_params...

  etcpal_error_t init_result = sacn_init(&log_params);
  // Or, to init without worrying about logs from the sACN library...
  etcpal_error_t init_result = sacn_init(NULL);
}

// Called at shutdown
void shutdown_sacn(void)
{
  sacn_deinit();
}
```

To create an sACN receiver instance, use the sacn_receiver_create() function. An sACN receiver
instance can listen on one universe at a time, but the universe it listens on can be changed at any
time.

The sACN Receiver API is an asynchronous, callback-oriented API. Part of the initial configuration
for a receiver instance is a set of function pointers for the library to use as callbacks.
Callbacks are dispatched from a background thread which is started when the first receiver instance
is created.

```c
SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
// Or, to initialize default values at runtime:
sacn_receiver_config_init(&config);

config.universe = 1; // Listen on universe 1

// Set the callback functions - defined elsewhere
config.callbacks.universe_data = my_universe_data_callback;
config.callbacks.sources_lost = my_sources_lost_callback;
config.callbacks.source_pcp_lost = my_source_pcp_lost_callback; // optional, can be NULL
config.callbacks.sampling_ended = my_sampling_ended_callback;
config.callbacks.source_limit_exceeded = my_source_limit_exceeded_callback; // optional, can be NULL

sacn_receiver_t my_receiver_handle;
etcpal_error_t result = sacn_receiver_create(&config, &my_receiver_handle);
if (result == kEtcPalErrOk)
{
  // Handle is valid and may be referenced in later calls to API functions.
}
else
{
  // Some error occurred, handle is not valid.
}
```

## Receiving sACN Data

Each time sACN data is received on a universe that is being listened to, it will be forwarded via
the corresponding `universe_data()` callback.

```c
void my_universe_data_callback(sacn_receiver_t handle, const EtcPalSockAddr* source_addr, const SacnHeaderData* header,
                               const uint8_t* pdata, void* context)
{
  // Check handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  char addr_str[ETCPAL_INET6_ADDRSTRLEN];
  etcpal_inet_ntop(&source_addr->ip, addr_str, ETCPAL_INET6_ADDRSTRLEN);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&header->cid, cid_str);

  printf("Got sACN update from source %s (address %s:%u, name %s) on universe %u, priority %u, start code %u\n",
         cid_str, addr_str, source_addr->port, header->source_name, header->universe_id, header->priority,
         header->start_code);

  // Example for an sACN-enabled fixture...
  if (header->start_code == 0 && my_start_addr + MY_DMX_FOOTPRINT <= header->slot_count)
  {
    memcpy(my_data_buf, &pdata[my_start_addr], MY_DMX_FOOTPRINT);
    // Act on the data somehow
  }
}
```

## The Sampling Period

Creating a receiver causes the library to immediately start listening for data on the specified
universe. For a period of time after the receiver is created, it is considered to be in a
_sampling period_. During this time, data received via the `universe_data()` callback should be
stored but not yet acted upon.

The sampling period exists because sACN allows multiple sources to send data to the same universe.
When first listening on a universe, you will get updates from any sources that are currently
sending on that universe in a non-deterministic order. Data from a source with lower priority might
be received before data from a high priority source; in this case, if the low-priority data is
acted upon before the high-priority data is received, a sudden jump in output might result. The
sampling period allows the receiver to gather the full set of information about all the sources
that are currently sending data on a universe before acting on any of the data.

The library provides a sampling_ended callback which is fired from a timer when the sampling period
ends. This callback is just for convenience; you can implement your own timer if you want. The
current sampling time is 1.5 seconds.

```c
void my_sampling_ended_callback(sacn_receiver_t handle, void* context)
{
  // Check handle and/or context as necessary...

  // Then, start acting upon any stored data from this universe.
}
```

## Tracking Sources

The sACN library tracks each source that is sending DMX data on a universe. In sACN, multiple
sources can send data simultaneously on the same universe. There are many ways to resolve
conflicting data from different sources and determine the winner; some tools for this are given by
the library and others can be implemented by the consuming application.

Each source has a _Component Identifier_ (CID), which is a UUID that is unique to that source. The
CID should be used as a primary key to differentiate sources. Sources also have a descriptive name
that can be user-assigned and is not required to be unique. This source data is provided in the
#SacnHeaderData struct in the `universe_data()` callback, as well as in the #SacnLostSource struct
in the `sources_lost()` callback.

### Priority

The sACN standard provides a priority value with each sACN data packet. This unsigned 8-bit value
ranges from 0 to 200 inclusive, where 0 is the lowest and 200 is the highest priority. The priority
value for a packet is available in the header structure that accompanies the `universe_data()`
callback. It is the application's responsibility to ensure that higher-priority data takes
precedence over lower-priority data.

ETC has extended the sACN standard with a method of tracking priority on a per-channel basis. This
is implemented by sending an alternate START code packet on the same universe periodically - the
START code is `0xdd`. When receiving a data packet with the start code `0xdd`, the slots in that
packet are to be interpreted as a priority value from 0 to 200 inclusive for the corresponding slot
in the DMX (NULL START code) packets. Receivers which wish to implement the per-channel priority
extension should check for the `0xdd` start code and handle the data accordingly.

When implementing the per-slot priority extension, the `source_pcp_lost()` callback should be
implemented to handle the condition where a source that was previously sending `0xdd` packets stops
sending them:

```c
void my_source_pcp_lost_callback(sacn_receiver_t handle, const SacnRemoteSource* source, void* context)
{
  // Check handle and/or context as necessary...

  // Revert to using the per-packet priority value to resolve priorities for this universe.
}
```

### Merging

When multiple sources are sending on the same universe at the same priority, receiver
implementations are responsible for designating a merge algorithm to merge the data from the
sources together. The most commonly-used algorithm is Highest Takes Precedence (HTP), where the
numerically highest value for each slot from any source is the one that is acted upon.

## Sources Lost Conditions

When a previously-tracked source stops sending data, the sACN library implements a custom data-loss
algorithm to attempt to group lost sources together. See \ref data_loss_behavior for more
information on this algorithm.

After the data-loss algorithm runs, the library will deliver a `sources_lost()` callback to
indicate that some number of sources have gone offline.

```c
void my_sources_lost_callback(sacn_receiver_t handle, const SacnLostSource* lost_sources, size_t num_lost_sources,
                              void* context)
{
  // Check handle and/or context as necessary...

  // You might not normally print a message on this condition, but this is just to demonstrate
  // the fields available:
  printf("The following sources have gone offline:\n")
  for (SacnLostSource* source = lost_sources; source < lost_sources + num_lost_sources; ++source)
  {
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    etcpal_uuid_to_string(&source->cid, cid_str);
    printf("CID: %s\tName: %s\tTerminated: %s\n", cid_str, source->name, source->terminated ? "true" : "false");

    // Remove the source from your state tracking...
  }
}
```

## Source Limit Exceeded Conditions

The sACN library will only forward data from sources that it is able to track. When the library is
compilied with #SACN_DYNAMIC_MEM set to 0, this means that it will only track up to
#SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE sources at a time. (You can change these compile options in
your sacn_config.h; see \ref building_and_integrating.)

When the library encounters a source that it does not have room to track, it will send a single
`source_limit_exceeded()` notification. Additional `source_limit_exceeded()` callbacks will not be
delivered until the number of tracked sources falls below the limit and then exceeds it again.

If sACN was compiled with #SACN_DYNAMIC_MEM set to 1 (the default on non-embedded platforms), the
library will track as many sources as it is able to dynamically allocate memory for, and this
callback will not be called in normal program operation (and can be set to NULL in the config
struct).

```c
void my_source_limit_exceeded_callback(sacn_receiver_t handle, void* context)
{
  // Check handle and/or context as necessary...

  // Handle the condition in an application-defined way. Maybe log it?
}
```
