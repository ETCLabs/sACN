# Using the sACN Receiver API                                                     {#using_receiver}

The sACN Receiver API provides a method for applications to receive and process sACN data on one or
more universes. This API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The sACN library must be globally initialized before using the Receiver API. See
\ref global_init_and_destroy.

An sACN receiver instance can listen on one universe at a time, but the universe it listens on can
be changed at any time. A receiver begins listening when it is created. To create an sACN receiver
instance, use the `sacn_receiver_create()` function in C, or instantiate an sacn::Receiver and call
its `Startup()` function in C++. A receiver can later be destroyed by calling
`sacn_receiver_destroy()` in C or `Shutdown()` in C++.

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
config.callbacks.universe_data = my_universe_data_callback;
config.callbacks.sources_lost = my_sources_lost_callback;
config.callbacks.sampling_period_ended = my_sampling_period_ended_callback;
config.callbacks.sampling_period_started = my_sampling_period_started_callback; // optional, can be NULL
config.callbacks.source_pap_lost = my_source_pap_lost_callback; // optional, can be NULL
config.callbacks.source_limit_exceeded = my_source_limit_exceeded_callback; // optional, can be NULL

SacnMcastInterface my_netints[NUM_MY_NETINTS];
// Assuming my_netints and NUM_MY_NETINTS are initialized by the application...

SacnNetintConfig netint_config;
netint_config.netints = my_netints;
netint_config.num_netints = NUM_MY_NETINTS;

sacn_receiver_t my_receiver_handle;

// If you want to specify specific network interfaces to use:
sacn_receiver_create(&config, &my_receiver_handle, &netint_config);
// Or, if you just want to use all network interfaces:
sacn_receiver_create(&config, &my_receiver_handle, NULL);
// You can add additional receivers as well, in the same way.

// To destroy the receiver when you're done with it:
sacn_receiver_destroy(my_receiver_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Implement the callback functions by inheriting sacn::Receiver::NotifyHandler:
class MyNotifyHandler : public sacn::Receiver::NotifyHandler
{
  // Required callbacks that must be implemented:
  void HandleUniverseData(Handle receiver_handle, const etcpal::SockAddr& source_addr,
                          const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data) override;
  void HandleSourcesLost(Handle handle, uint16_t universe, const std::vector<SacnLostSource>& lost_sources) override;

  // Optional callbacks - these don't have to be a part of MyNotifyHandler:
  void HandleSamplingPeriodStarted(Handle handle, uint16_t universe) override;
  void HandleSamplingPeriodEnded(Handle handle, uint16_t universe) override;
  void HandleSourcePapLost(Handle handle, uint16_t universe, const SacnRemoteSource& source) override;
  void HandleSourceLimitExceeded(Handle handle, uint16_t universe) override;
};

// Now to set up a receiver:
sacn::Receiver::Settings config(1); // Instantiate config & listen on universe 1
sacn::Receiver receiver; // Instantiate a receiver

MyNotifyHandler my_notify_handler;
receiver.Startup(config, my_notify_handler);
// Or do this if Startup is being called within the NotifyHandler-derived class:
receiver.Startup(config, *this);
// Or do this to specify custom interfaces for the receiver to use:
std::vector<SacnMcastInterface> my_netints;  // Assuming my_netints is initialized by the application...
receiver.Startup(config, my_notify_handler, my_netints);

// To destroy the receiver when you're done with it:
receiver.Shutdown();
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
sacn_receiver_get_universe(my_receiver_handle, &current_universe);

// Change the universe to listen to
uint16_t new_universe = current_universe + 1;
sacn_receiver_change_universe(my_receiver_handle, new_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get the universe currently being listened to
auto result = receiver.GetUniverse();
if (result)
{
  // Change the universe to listen to
  uint16_t new_universe = *result + 1;
  receiver.ChangeUniverse(new_universe);
}
```
<!-- CODE_BLOCK_END -->

### Footprints

TODO: Custom footprints are not yet implemented, so the footprint will always be the full universe.

A receiver can also be configured to listen to a specific range of slots within the universe, which
is called the footprint. For example, networked fixtures might use this to only retrieve data about
slots within their DMX footprint. The footprint is initially specified in the receiver config, but
it is optional, so it defaults to the full universe if it isn't specified in the config. There are
also functions to get and change the footprint, including a function that can change both the
universe and footprint at once.

<!-- CODE_BLOCK_START -->
```c
// Get the current footprint
SacnRecvUniverseSubrange current_footprint;
sacn_receiver_get_footprint(my_receiver_handle, &current_footprint);

// Change the footprint, but keep the universe the same
SacnRecvUniverseSubrange new_footprint;
new_footprint.start_address = 20;
new_footprint.address_count = 10;
sacn_receiver_change_footprint(my_receiver_handle, &new_footprint);

// Change both the universe and the footprint at once
uint16_t new_universe = current_universe + 1;
new_footprint.start_address = 40;
new_footprint.address_count = 20;
sacn_receiver_change_universe_and_footprint(my_receiver_handle, new_universe, &new_footprint);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get the universe currently being listened to
auto current_footprint = receiver.GetFootprint();
if (current_footprint)
{
  // Change the footprint, but keep the universe the same
  SacnRecvUniverseSubrange new_footprint;
  new_footprint.start_address = current_footprint.start_address + 10;
  new_footprint.address_count = current_footprint.address_count;
  receiver.ChangeFootprint(new_footprint);

  // Change both the universe and the footprint at once
  new_footprint.start_address += 10;
  uint16_t new_universe = 100u;
  receiver.ChangeUniverseAndFootprint(new_universe, new_footprint);
}
```
<!-- CODE_BLOCK_END -->

## Receiving sACN Data

The universe data callback forwards incoming DMX data to the application. See the callback's
documentation for more information about when it is called and when it isn't.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                               const SacnRemoteSource* source_info, const SacnRecvUniverseData* universe_data,
                               void* context)
{
  // Check receiver_handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&source_addr->ip, addr_str);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&source_info->cid, cid_str);

  printf("Got sACN update from source %s (address %s:%u, name %s) on universe %u, priority %u, start code %u",
         cid_str, addr_str, source_addr->port, source_info->name, universe_data->universe_id, universe_data->priority,
         universe_data->start_code);

  if (universe_data->is_sampling)
    printf(" (during the sampling period)\n");
  else
    printf("\n");

  // Example for an sACN-enabled fixture...
  if ((universe_data->start_code == 0x00) && (universe_data->slot_range.start_address == my_start_addr) &&
      (universe_data->slot_range.address_count == MY_DMX_FOOTPRINT))
  {
    // values[0] will always be the level of the first slot of the footprint
    memcpy(my_data_buf, universe_data->values, MY_DMX_FOOTPRINT);
    // Act on the data somehow
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleUniverseData(Handle receiver_handle, const etcpal::SockAddr& source_addr,
                                         const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  std::cout << "Got sACN update from source " << etcpal::Uuid(source_info.cid).ToString() << " (address " 
            << source_addr.ToString() << ", name " << source_info.name << ") on universe "
            << universe_data.universe_id << ", priority " << universe_data.priority << ", start code "
            << universe_data.start_code;

  if (universe_data.is_sampling)
    std::cout << " (during the sampling period)\n";
  else
    std::cout << "\n";

  // Example for an sACN-enabled fixture...
  if ((universe_data.start_code == 0x00) && (universe_data.slot_range.start_address == my_start_addr) &&
      (universe_data.slot_range.address_count == MY_DMX_FOOTPRINT))
  {
    // values[0] will always be the level of the first slot of the footprint
    memcpy(my_data_buf, universe_data.values, MY_DMX_FOOTPRINT);
    // Act on the data somehow
  }
}
```
<!-- CODE_BLOCK_END -->

## The Sampling period

There may be multiple sources transmitting data on a universe. The sampling period is used in order
to remove flicker as sources are discoverd. There are notifications for when the sampling period
begins, as well as when it ends, for each universe. The Universe Data callback also provides
is_sampling (in universe_data) to indicate if the data was received during the sampling period.
These notifications allow the application to know when to act on the universe data with assurance
that all of the current sources are represented.

The sampling period occurs when a new receiver is created, as well as when the universe and/or
footprint are changed.

Here is an example of the Sampling Period Ended callback:

<!-- CODE_BLOCK_START -->
```c
void my_sampling_period_ended_callback(sacn_receiver_t handle, uint16_t universe, void* context)
{
  // Check handle and/or context as necessary...

  // Apply universe data as needed...
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSamplingPeriodEnded(Handle handle, uint16_t universe)
{
  // Apply universe data as needed...
}
```
<!-- CODE_BLOCK_END -->

## Tracking Sources

The sACN library tracks each source that is sending DMX data on a universe. In sACN, multiple
sources can send data simultaneously on the same universe. There are many ways to resolve
conflicting data from different sources and determine the winner; some tools for this are given by
the library and others can be implemented by the consuming application.

Each source has a _Component Identifier_ (CID), which is a UUID that is unique to that source. Each
source also has a handle that should be used as a primary key, differentiating it from other
sources. Sources also have a descriptive name that can be user-assigned and is not required to be
unique. This source data is provided in the #SacnRemoteSource struct in the universe data callback,
as well as in the #SacnLostSource struct in the sources lost callback.

### Priority

The sACN standard provides a universe priority value with each sACN data packet. This unsigned
8-bit value ranges from 0 to 200, where 0 is the lowest and 200 is the highest priority. The
priority value for a packet is available in the universe_data parameter of the universe data
callback. It is the application's responsibility to ensure that higher-priority data takes
precedence over lower-priority data.

ETC has extended the sACN standard with a method of tracking priority on a per-address basis. This
is implemented by sending an alternate START code packet on the same universe periodically - the
START code is `0xdd`. When receiving a data packet with the start code `0xdd`, the values in that
packet are to be interpreted as a priority value from 1 to 200 for the corresponding level in the
DMX (NULL START code) packets. A value of 0 indicates "not sourced", meaning the source is not
sending NULL START code data to that slot. Likewise, if less than 512 `0xdd` values are received,
then the values beyond the last `0xdd` slot should also be treated as not sourced. Receivers which
wish to implement the per-address priority extension should make sure that
#SACN_ETC_PRIORITY_EXTENSION is set to 1, check for the `0xdd` start code, and handle the data
accordingly.

When implementing the per-address priority extension, the source PAP lost callback should be
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
void MyNotifyHandler::HandleSourcePapLost(Handle handle, uint16_t universe, const SacnRemoteSource& source)
{
  // Revert to using the per-packet priority value to resolve priorities for this universe.
}
```
<!-- CODE_BLOCK_END -->

If the library is compiled with #SACN_ETC_PRIORITY_EXTENSION set to 0, then `0xdd` packets will not
be received, and the source PAP lost callback will not be called.

### Merging

When multiple sources are sending on the same universe at the same priority, receiver
implementations are responsible for designating a merge algorithm to merge the data from the
sources together. The most commonly-used algorithm is Highest Takes Precedence (HTP), where the
numerically highest level for each slot from any source is the one that is acted upon.

The sACN library provides a couple of APIs to facilitate merging. The first, the DMX merger API,
provides a software merger that takes NULL start code and priority data as input and outputs the
merged levels, along with source IDs and PAP for each level. For more information, see \ref
using_dmx_merger. The second, the merge receiver API, combines the receiver and DMX merger APIs to
offer a simplified solution for receiver functionality with merging built in. See \ref
using_merge_receiver for more information.

## Sources Lost Conditions

When a previously-tracked source stops sending data, the sACN library implements a custom source-loss
algorithm to attempt to group lost sources together. See \ref source_loss_behavior for more
information on this algorithm.

After the source-loss algorithm runs, the library will deliver a sources lost callback to indicate
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
void MyNotifyHandler::HandleSourcesLost(Handle handle, uint16_t universe,
                                        const std::vector<SacnLostSource>& lost_sources)
{
  // You might not normally print a message on this condition, but this is just to demonstrate
  // the fields available:
  std::cout << "The following sources have gone offline:\n";
  for(const SacnLostSource& src : lost_sources)
  {
    std::cout << "CID: " << etcpal::Uuid(source.cid).ToString() << "\tName: " << source.name << "\tTerminated: " 
              << source.terminated << "\n";

    // Remove the source from your state tracking...
  }
}
```
<!-- CODE_BLOCK_END -->

If a source that was lost comes back again, then there will once again be universe data
notifications for that source.

## Source Limit Exceeded Conditions

The sACN library will only forward data from sources that it is able to track. When the library is
compilied with #SACN_DYNAMIC_MEM set to 0, this means that it will only track up to
#SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE sources at a time. (You can change these compile options in
your sacn_config.h; see \ref building_and_integrating.)

When the library encounters a source that it does not have room to track, it will send a single
source limit exceeded notification. Additional source limit exceeded callbacks will not be
delivered until the number of tracked sources falls below the limit and then exceeds it again.

The library will also check against the `source_count_max` value from the receiver config/settings,
limiting it to #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE if #SACN_DYNAMIC_MEM is 0. The
`source_count_max` value may be set to #SACN_RECEIVER_INFINITE_SOURCES, which is the default. In
this case, if #SACN_DYNAMIC_MEM is 0, #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE will be the limit.
Otherwise, if #SACN_DYNAMIC_MEM is 1, the library will track as many sources as it is able to
dynamically allocate memory for, and this callback will not be called in normal program operation
(and can be set to NULL in the config struct in C).

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
void MyNotifyHandler::HandleSourceLimitExceeded(Handle handle, uint16_t universe)
{
  // Handle the condition in an application-defined way. Maybe log it?
}
```
<!-- CODE_BLOCK_END -->
