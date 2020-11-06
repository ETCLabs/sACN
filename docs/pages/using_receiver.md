# Using the sACN Receiver API                                                     {#using_receiver}

The sACN Receiver API provides a method for applications to receive and process sACN data on one or
more universes. This API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The sACN library has overall init and deinit functions that should be called once each at
application startup and shutdown time. These functions interface with the EtcPal \ref etcpal_log
API to configure what happens when the sACN library logs messages. Optionally pass an
EtcPalLogParams structure to use this functionality. This structure can be shared across different
ETC library modules.

<!-- CODE_BLOCK_START -->
```c
#include "sacn/receiver.h"

// During startup:
EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
// Initialize log_params...

etcpal_error_t init_result = sacn_init(&log_params);
// Or, to init without worrying about logs from the sACN library...
etcpal_error_t init_result = sacn_init(NULL);

// During shutdown:
sacn_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "sacn/cpp/receiver.h"

// During startup:
etcpal::Logger logger;
// Initialize logger...

etcpal::Error init_result = sacn::Init(logger);
// Or, to init without worrying about logs from the sACN library...
etcpal::Error init_result = sacn::Init();

// During shutdown:
sacn::Deinit();
```
<!-- CODE_BLOCK_END -->

An sACN receiver instance can listen on one universe at a time, but the universe it listens on can
be changed at any time. A receiver begins listening when it is created. To create an sACN receiver
instance, use the `sacn_receiver_create()` function in C, or instantiate an sacn::Receiver and call
its `Startup()` function in C++.

The sACN Receiver API is an asynchronous, callback-oriented API. Part of the initial configuration
for a receiver instance is to specify the callbacks for the library to use. In C, these are
specified as a set of function pointers. In C++, these are specified as an instance of a class that
implements sacn::Receiver::NotifyHandler. Callbacks are dispatched from a background thread which
is started when the first receiver instance is created.

<!-- CODE_BLOCK_START -->
```c
SacnReceiverConfig config = SACN_RECEIVER_CONFIG_DEFAULT_INIT;
// Or, to initialize default values at runtime:
sacn_receiver_config_init(&config);

config.universe_id = 1; // Listen on universe 1

// Set the callback functions - defined elsewhere
config.callbacks.sources_found = my_sources_found_callback;
config.callbacks.universe_data = my_universe_data_callback;
config.callbacks.sources_lost = my_sources_lost_callback;
config.callbacks.source_pap_lost = my_source_pap_lost_callback; // optional, can be NULL
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
<!-- CODE_BLOCK_MID -->
```cpp
sacn::Receiver::Settings config(1); // Instantiate config & listen on universe 1
sacn::Receiver receiver; // Instantiate a receiver

// You implement the callbacks by implementing the sacn::Receiver::NotifyHandler interface.
// The NotifyHandler-derived instance, referred to here as my_notify_handler, must be passed in to Startup:
MyNotifyHandler my_notify_handler;
etcpal::Error startup_result = receiver.Startup(config, my_notify_handler);
// Or do this if Startup is being called within the NotifyHandler-derived class:
etcpal::Error startup_result = receiver.Startup(config, *this);

if (startup_result)
{
  // The receiver is initialized and can now be used.
}
else
{
  // Some error occurred, the receiver is not valid.
}
```
<!-- CODE_BLOCK_END -->

## Listening on a Universe

A receiver can listen to one universe at a time. The initial universe being listened to is
specified in the config passed into create/Startup. There are functions that allow you to get the
current universe being listened to, or to change the universe to listen to.

<!-- CODE_BLOCK_START -->
```c
// Get the universe currently being listened to
uint16_t current_universe;
etcpal_error_t result = sacn_receiver_get_universe(my_receiver_handle, &current_universe);

// Change the universe to listen to
uint16_t new_universe = current_universe + 1;
etcpal_error_t result = sacn_receiver_change_universe(my_receiver_handle, new_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get the universe currently being listened to
uint16_t current_universe;
auto result = receiver.GetUniverse();

if(result)
  current_universe = *result;

// Change the universe to listen to
uint16_t new_universe = current_universe + 1;
etcpal::Error result = receiver.ChangeUniverse(new_universe);
```
<!-- CODE_BLOCK_END -->

## Finding New Sources

There may be multiple sources transmitting data on a universe. A sources found notification occurs
whenever a new set of sources has been found, and the correct starting values for NULL start code
data and per-address priority have been determined for each.

<!-- CODE_BLOCK_START -->
```c
void my_sources_found_callback(sacn_receiver_t handle, uint16_t universe, const SacnFoundSource* found_sources,
                               size_t num_found_sources, void* context)
{
  // Check handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  for(const SacnFoundSource* src = found_sources; src < (found_sources + num_found_sources); ++src)
  {
    char addr_str[ETCPAL_IP_STRING_BYTES];
    etcpal_ip_to_string(&src->from_addr.ip, addr_str);

    char cid_str[ETCPAL_UUID_STRING_BYTES];
    etcpal_uuid_to_string(&src->cid, cid_str);

    printf("Found new source %s (address %s:%u, name %s) on universe %u\n", cid_str, addr_str, src->from_addr.port,
           src->name, universe);

    printf("Starting values:\n Preview: %s\n Per-universe priority: %u\n", src->preview ? "true" : "false", 
           src->priority);

    for(unsigned int i = 0; i < src->values_len; ++i)
      printf(" NULL start code data, slot %u: %u\n", i, src->values[i]);
    for(unsigned int i = 0; i < src->per_address_len; ++i)
      printf(" Per-address priority, slot %u: %u\n", i, src->per_address[i]);
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourcesFound(uint16_t universe, const SacnFoundSource* found_sources, 
                                         size_t num_found_sources)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  for(const SacnFoundSource* src = found_sources; src < (found_sources + num_found_sources); ++src)
  {
    std::cout << "Found new source " << etcpal::Uuid(src->cid).ToString() << " (address " 
              << etcpal::IpAddr(src->from_addr.ip).ToString() << ":" << src->from_addr.port << ", name " << src->name 
              << ") on universe " << universe << "\n";

    std::cout << "Starting values:\n Preview: " << src->preview << "\n Per-universe priority: " << src->priority 
              << "\n";

    for(unsigned int i = 0; i < src->values_len; ++i)
      std::cout << " NULL start code data, slot " << i << ": " << src->values[i] << "\n";
    for(unsigned int i = 0; i < src->per_address_len; ++i)
      std::cout << " Per-address priority, slot " << i << ": " << src->per_address[i] << "\n";
  }
}
```
<!-- CODE_BLOCK_END -->

## Receiving sACN Data

Each time DMX data is received on a universe that is being listened to, from a source that was
already included in a sources found notification, it will be forwarded via the corresponding
universe data callback.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_receiver_t handle, uint16_t universe, const EtcPalSockAddr* source_addr, 
                               const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  // Check handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&source_addr->ip, addr_str);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&header->cid, cid_str);

  printf("Got sACN update from source %s (address %s:%u, name %s) on universe %u, priority %u, start code %u\n",
         cid_str, addr_str, source_addr->port, header->source_name, universe, header->priority, header->start_code);

  // Example for an sACN-enabled fixture...
  if (header->start_code == 0 && my_start_addr + MY_DMX_FOOTPRINT <= header->slot_count)
  {
    memcpy(my_data_buf, &pdata[my_start_addr], MY_DMX_FOOTPRINT);
    // Act on the data somehow
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleUniverseData(uint16_t universe, const etcpal::SockAddr& source_addr,
                                         const SacnHeaderData& header, const uint8_t* pdata)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  std::cout << "Got sACN update from source " << etcpal::Uuid(header.cid).ToString() << " (address " 
            << source_addr.ToString() << ", name " << header.source_name << ") on universe " << universe 
            << ", priority " << header.priority << ", start code " << header.start_code << "\n";

  // Example for an sACN-enabled fixture...
  if (header.start_code == 0 && my_start_addr + MY_DMX_FOOTPRINT <= header.slot_count)
  {
    memcpy(my_data_buf, &pdata[my_start_addr], MY_DMX_FOOTPRINT);
    // Act on the data somehow
  }
}
```
<!-- CODE_BLOCK_END -->

## Tracking Sources

The sACN library tracks each source that is sending DMX data on a universe. In sACN, multiple
sources can send data simultaneously on the same universe. There are many ways to resolve
conflicting data from different sources and determine the winner; some tools for this are given by
the library and others can be implemented by the consuming application.

Each source has a _Component Identifier_ (CID), which is a UUID that is unique to that source. The
CID should be used as a primary key to differentiate sources. Sources also have a descriptive name
that can be user-assigned and is not required to be unique. This source data is provided in the
#SacnFoundSource struct in the sources found callback, the #SacnHeaderData struct in the universe
data callback, as well as in the #SacnLostSource struct in the sources lost callback.

### Priority

The sACN standard provides a priority value with each sACN data packet. This unsigned 8-bit value
ranges from 0 to 200 inclusive, where 0 is the lowest and 200 is the highest priority. The priority
value for a packet is available in the header structure that accompanies the universe data
callback. It is the application's responsibility to ensure that higher-priority data takes
precedence over lower-priority data.

ETC has extended the sACN standard with a method of tracking priority on a per-address basis. This
is implemented by sending an alternate START code packet on the same universe periodically - the
START code is `0xdd`. When receiving a data packet with the start code `0xdd`, the slots in that
packet are to be interpreted as a priority value from 0 to 200 inclusive for the corresponding slot
in the DMX (NULL START code) packets. Receivers which wish to implement the per-address priority
extension should check for the `0xdd` start code and handle the data accordingly.

When implementing the per-slot priority extension, the source PAP lost callback should be
implemented to handle the condition where a source that was previously sending `0xdd` packets stops
sending them:

<!-- CODE_BLOCK_START -->
```c
void my_source_pap_lost_callback(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, 
                                 void* context)
{
  // Check handle and/or context as necessary...

  // Revert to using the per-packet priority value to resolve priorities for this universe.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourcePapLost(uint16_t universe, const SacnRemoteSource& source)
{
  // Revert to using the per-packet priority value to resolve priorities for this universe.
}
```
<!-- CODE_BLOCK_END -->

### Merging

When multiple sources are sending on the same universe at the same priority, receiver
implementations are responsible for designating a merge algorithm to merge the data from the
sources together. The most commonly-used algorithm is Highest Takes Precedence (HTP), where the
numerically highest value for each slot from any source is the one that is acted upon.

The sACN library provides a couple of APIs to facilitate merging. The first, the DMX merger API,
provides a software merger that takes start code 0 and PAP data as input and outputs the merged
levels, along with source IDs for each level. For more information, see \ref using_dmx_merger.
The second, the merge receiver API, combines the receiver and DMX merger APIs to offer a simplified
solution for receiver functionality with merging built in. See \ref using_merge_receiver for
more information.

## Sources Lost Conditions

When a previously-tracked source stops sending data, the sACN library implements a custom data-loss
algorithm to attempt to group lost sources together. See \ref data_loss_behavior for more
information on this algorithm.

After the data-loss algorithm runs, the library will deliver a sources lost callback to indicate
that some number of sources have gone offline.

<!-- CODE_BLOCK_START -->
```c
void my_sources_lost_callback(sacn_receiver_t handle, uint16_t universe, const SacnLostSource* lost_sources, 
                              size_t num_lost_sources, void* context)
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
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourcesLost(uint16_t universe, const SacnLostSource* lost_sources, size_t num_lost_sources)
{
  // You might not normally print a message on this condition, but this is just to demonstrate
  // the fields available:
  std::cout << "The following sources have gone offline:\n";
  for (SacnLostSource* source = lost_sources; source < lost_sources + num_lost_sources; ++source)
  {
    std::cout << "CID: " << etcpal::Uuid(source->cid).ToString() << "\tName: " << source->name << "\tTerminated: " 
              << source->terminated << "\n";

    // Remove the source from your state tracking...
  }
}
```
<!-- CODE_BLOCK_END -->

If a source that was lost comes back again, then it will be included in another sources found
notification.

## Source Limit Exceeded Conditions

The sACN library will only forward data from sources that it is able to track. When the library is
compilied with #SACN_DYNAMIC_MEM set to 0, this means that it will only track up to
#SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE sources at a time. (You can change these compile options in
your sacn_config.h; see \ref building_and_integrating.)

When the library encounters a source that it does not have room to track, it will send a single
source limit exceeded notification. Additional source limit exceeded callbacks will not be
delivered until the number of tracked sources falls below the limit and then exceeds it again.

If sACN was compiled with #SACN_DYNAMIC_MEM set to 1 (the default on non-embedded platforms), the
library will check against the `source_count_max` value from the receiver config/settings, instead
of #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE. The `source_count_max` value may be set to
#SACN_RECEIVER_INFINITE_SOURCES, in which case the library will track as many sources as it is able
to dynamically allocate memory for, and this callback will not be called in normal program
operation (and can be set to NULL in the config struct in C).

<!-- CODE_BLOCK_START -->
```c
void my_source_limit_exceeded_callback(sacn_receiver_t handle, uint16_t universe, void* context)
{
  // Check handle and/or context as necessary...

  // Handle the condition in an application-defined way. Maybe log it?
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourceLimitExceeded(uint16_t universe)
{
  // Handle the condition in an application-defined way. Maybe log it?
}
```
<!-- CODE_BLOCK_END -->
