# Using the sACN Merge Receiver API                                         {#using_merge_receiver}

The sACN Merge Receiver API combines the functionality of the sACN Receiver and DMX Merger APIs.
It provides the application with merged DMX levels, plus data with other start codes from each
source, as sACN packets are received. This API exposes both a C and C++ language interface. The
C++ interface is a header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The sACN library must be globally initialized before using the Merge Receiver API. See
\ref global_init_and_destroy.

An sACN merge receiver instance can listen on one universe at a time, but the universe it listens
on can be changed at any time. A merge receiver begins listening when it is created. To create an
sACN merge receiver instance, use the `sacn_merge_receiver_create()` function in C, or instantiate
an sacn::MergeReceiver and call its `Startup()` function in C++. A merge receiver can later be
destroyed by calling `sacn_merge_receiver_destroy()` in C or `Shutdown()` in C++.

The sACN Merge Receiver API is an asynchronous, callback-oriented API. Part of the initial
configuration for a merge receiver instance is to specify the callbacks for the library to use. In
C, these are specified as a set of function pointers. In C++, these are specified as an instance
of a class that implements sacn::MergeReceiver::NotifyHandler. Callbacks are dispatched from a
background thread which is started when the first merge receiver instance is created.

<!-- CODE_BLOCK_START -->
```c
SacnMergeReceiverConfig config = SACN_MERGE_RECEIVER_CONFIG_DEFAULT_INIT;
// Or, to initialize default values at runtime:
sacn_merge_receiver_config_init(&config);

config.universe_id = 1; // Listen on universe 1

// Set the callback functions - defined elsewhere
config.callbacks.universe_data = my_universe_data_callback;
config.callbacks.universe_non_dmx = my_universe_non_dmx_callback;
config.callbacks.sources_lost = my_sources_lost_callback;
config.callbacks.sampling_period_started = my_sampling_started_callback;
config.callbacks.sampling_period_ended = my_sampling_ended_callback;
config.callbacks.source_limit_exceeded = my_source_limit_exceeded_callback; // optional, can be NULL

SacnMcastInterface my_netints[NUM_MY_NETINTS];
// Assuming my_netints and NUM_MY_NETINTS are initialized by the application...

SacnNetintConfig netint_config = SACN_NETINT_CONFIG_DEFAULT_INIT;
netint_config.netints = my_netints;
netint_config.num_netints = NUM_MY_NETINTS;

sacn_merge_receiver_t my_merge_receiver_handle;

// If you want to specify specific network interfaces to use:
sacn_merge_receiver_create(&config, &my_merge_receiver_handle, &netint_config);
// Or, if you just want to use all network interfaces:
sacn_merge_receiver_create(&config, &my_merge_receiver_handle, NULL);
// You can add additional merge receivers as well, in the same way.

// To destroy the merge receiver when you're done with it:
sacn_merge_receiver_destroy(my_merge_receiver_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Implement the callback functions by inheriting sacn::MergeReceiver::NotifyHandler:
class MyNotifyHandler : public sacn::MergeReceiver::NotifyHandler
{
  // Required callbacks that must be implemented:
  void HandleMergedData(sacn::MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data) override;
  void HandleNonDmxData(sacn::MergeReceiver::Handle receiver_handle, const etcpal::SockAddr& source_addr,
                        const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data) override;

  // Optional callbacks - these don't have to be a part of MyNotifyHandler:
  void HandleSourcesLost(sacn::MergeReceiver::Handle handle, uint16_t universe, const std::vector<SacnLostSource>& lost_sources) override;
  void HandleSamplingPeriodStarted(sacn::MergeReceiver::Handle handle, uint16_t universe) override;
  void HandleSamplingPeriodEnded(sacn::MergeReceiver::Handle handle, uint16_t universe) override;
  void HandleSourceLimitExceeded(sacn::MergeReceiver::Handle handle, uint16_t universe) override;
};

// Now to set up a merge receiver:
sacn::MergeReceiver::Settings config(1); // Instantiate config & listen on universe 1
sacn::MergeReceiver merge_receiver; // Instantiate a merge receiver

MyNotifyHandler my_notify_handler;
merge_receiver.Startup(config, my_notify_handler);
// Or do this if Startup is being called within the NotifyHandler-derived class:
merge_receiver.Startup(config, *this);
// Or do this to specify custom interfaces for the merge receiver to use:
std::vector<SacnMcastInterface> my_netints;  // Assuming my_netints is initialized by the application...
merge_receiver.Startup(config, my_notify_handler, my_netints);

// To destroy the merge receiver when you're done with it:
merge_receiver.Shutdown();
```
<!-- CODE_BLOCK_END -->

## Listening on a Universe

A merge receiver can listen to one universe at a time. The initial universe being listened to is
specified in the config passed into create/Startup. There are functions that allow you to get the
current universe being listened to, or to change the universe to listen to.

<!-- CODE_BLOCK_START -->
```c
// Get the universe currently being listened to
uint16_t current_universe;
sacn_merge_receiver_get_universe(my_merge_receiver_handle, &current_universe);

// Change the universe to listen to
uint16_t new_universe = current_universe + 1;
sacn_merge_receiver_change_universe(my_merge_receiver_handle, new_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get the universe currently being listened to
auto result = merge_receiver.GetUniverse();
if (result)
{
  // Change the universe to listen to
  uint16_t new_universe = *result + 1;
  merge_receiver.ChangeUniverse(new_universe);
}
```
<!-- CODE_BLOCK_END -->

### Footprints

TODO: Custom footprints are not yet implemented, so the footprint will always be the full universe.

A merge receiver can also be configured to listen to a specific range of slots within the universe,
which is called the footprint. For example, networked fixtures might use this to only retrieve data
about slots within their DMX footprint. The footprint is initially specified in the merge receiver
config, but it is optional, so it defaults to the full universe if it isn't specified in the
config. There are also functions to get and change the footprint, including a function that can
change both the universe and footprint at once.

<!-- CODE_BLOCK_START -->
```c
// Get the current footprint
SacnRecvUniverseSubrange current_footprint;
sacn_merge_receiver_get_footprint(my_receiver_handle, &current_footprint);

// Change the footprint, but keep the universe the same
SacnRecvUniverseSubrange new_footprint;
new_footprint.start_address = 20;
new_footprint.address_count = 10;
sacn_merge_receiver_change_footprint(my_receiver_handle, &new_footprint);

// Change both the universe and the footprint at once
uint16_t new_universe = current_universe + 1;
new_footprint.start_address = 40;
new_footprint.address_count = 20;
sacn_merge_receiver_change_universe_and_footprint(my_receiver_handle, new_universe, &new_footprint);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get the universe currently being listened to
auto current_footprint = merge_receiver.GetFootprint();
if (current_footprint)
{
  // Change the footprint, but keep the universe the same
  SacnRecvUniverseSubrange new_footprint;
  new_footprint.start_address = current_footprint.start_address + 10;
  new_footprint.address_count = current_footprint.address_count;
  merge_receiver.ChangeFootprint(new_footprint);

  // Change both the universe and the footprint at once
  new_footprint.start_address += 10;
  uint16_t new_universe = 100u;
  merge_receiver.ChangeUniverseAndFootprint(new_universe, new_footprint);
}
```
<!-- CODE_BLOCK_END -->

## Merging

Once a merge receiver has been created, it will begin listening for data on the configured universe
and footprint. When DMX level or priority data arrives, or a source is lost, it performs a merge.
This is where it determines the correct DMX level and source for each slot of the footprint, since
there may be multiple sources transmitting on the same universe. It does this by selecting the
source with the highest priority. If there are multiple sources at that priority, then it selects
the source with the highest level. That is the Highest Takes Precedence (HTP) merge algorithm.

## Receiving sACN Data

The merged data callback is called whenever there are new merge results. These results only include
sources that are not in a sampling period. The sampling period occurs when a receiver starts
listening on a new universe, new footprint, or new set of interfaces. While a source is part of a
sampling period, its universe data is merged as it comes in, but won't be included in a merged data
notification until after the sampling period ends. This removes flicker as various sources in the
network are discovered. Some sources might not be part of a sampling period. In that case, their
universe data will continue being included in merged data notifications during the period.

This callback will be called in multiple ways:

1. When a new non-preview data packet or per-address priority packet is received from the sACN 
Receiver module, it is immediately and synchronously passed to a DMX Merger. If the sampling
period has not ended for the source, the merged result is not passed to this callback until the
sampling period ends. Otherwise, it is immediately and synchronously passed to this callback.

2. When a sACN source is no longer sending non-preview data or per-address priority packets, the
lost source callback from the sACN Receiver module will be passed to a merger, after which the
merged result is passed to this callback pending the sampling period.

Please note that per-address priority is an ETC-specific sACN extension, and is disabled if the
library is compiled with #SACN_ETC_PRIORITY_EXTENSION set to 0 (in which case per-address priority
packets received have no effect).

The merger will prioritize sources with the highest per-address priority (or universe priority if
the source doesn't provide per-address priorities). If two sources have the same highest priority,
the one with the highest NULL start code level wins (HTP).

There is a key distinction in how the merger interprets the lowest priority. The lowest universe
priority is 0, but the lowest per-address priority is 1. This is because a per-address priority of
0 indicates that the source is not sending any levels to the corresponding slot. The solution the
merger uses is to always track priorities per-address. If a source only has a universe priority,
that priority is used for each slot, except if it equals 0 - in that case, it is converted to 1.
This means the merger will treat a universe priority of 0 as equivalent to a universe priority of
1, as well as per-address priorities equal to 1.

Also keep in mind that if less than 512 per-address priorities are received, then the remaining
slots will be treated as if they had a per-address priority of 0. Likewise, if less than 512 levels
are received, the remaining slots will be treated as if they had a level of 0, but they may still
have non-zero priorities.

This callback should be processed quickly, since it will interfere with the receipt and processing
of other sACN packets on the universe.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data, void* context)
{
  // Check handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  printf("Got new merge results on universe %u\n", merged_data->universe_id);

  // Example for an sACN-enabled fixture...
  for (int i = 0; i < merged_data->slot_range.address_count; ++i)  // For each slot in my DMX footprint
  {
    // merged_data->owners[0] always represents the owner of the first slot in the footprint
    if (merged_data->owners[i] == SACN_REMOTE_SOURCE_INVALID)
    {
      // One of the slots in my DMX footprint does not have a valid source
      return;
    }
  }

  // merged_data->levels[0] will always be the level of the first slot of the footprint
  memcpy(my_data_buf, merged_data->levels, merged_data->slot_range.address_count);
  // Act on the data somehow
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleMergedData(sacn::MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  std::cout << "Got new merge results on universe " << merged_data.universe_id << "\n";

  // Example for an sACN-enabled fixture...
  for (int i = 0; i < merged_data.slot_range.address_count; ++i)  // For each slot in my DMX footprint
  {
    // merged_data.owners[0] always represents the owner of the first slot in the footprint
    if (merged_data.owners[i] == kInvalidRemoteSourceHandle)
    {
      // One of the slots in my DMX footprint does not have a valid source
      return;
    }
  }

  // merged_data.levels[0] will always be the level of the first slot of the footprint
  memcpy(my_data_buf, merged_data.levels, merged_data.slot_range.address_count);
  // Act on the data somehow
}
```
<!-- CODE_BLOCK_END -->

If non-NULL start code, non-PAP sACN data is received, it is passed through directly to the
non-DMX data callback.

<!-- CODE_BLOCK_START -->
```c
void my_universe_non_dmx_callback(sacn_merge_receiver_t receiver_handle, const EtcPalSockAddr* source_addr,
                                  const SacnRemoteSource* source_info, const SacnRecvUniverseData* universe_data,
                                  void* context)
{
  // Check receiver_handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&source_addr->ip, addr_str);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&source_info->cid, cid_str);

  printf("Got non-DMX sACN update from source %s (address %s:%u, name %s) on universe %u, priority %u, start code %u\n",
         cid_str, addr_str, source_addr->port, source_info->name, universe_data->universe_id, universe_data->priority,
         universe_data->start_code);

  // Act on the data somehow
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleNonDmxData(sacn::MergeReceiver::Handle receiver_handle, const etcpal::SockAddr& source_addr,
                                       const SacnRemoteSource& source_info, const SacnRecvUniverseData& universe_data)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  std::cout << "Got non-DMX sACN update from source " << etcpal::Uuid(source_info.cid).ToString() << " (address " 
            << source_addr.ToString() << ", name " << source_info.name << ") on universe " << universe_data.universe_id 
            << ", priority " << universe_data.priority << ", start code " << universe_data.start_code << "\n";

  // Act on the data somehow
}
```
<!-- CODE_BLOCK_END -->

## Tracking Sources

The data callbacks include data originating from one or more sources transmitting on the current
universe. Each source has a handle that serves as a primary key, differentiating it from other
sources. The merge receiver provides information about each source that's active on the current
universe, including the name, IP, and CID. This information is provided directly in the non-DMX
callback. However, in the merged data callback, only the source handles are passed in. To obtain
more details about a source, use the get source function.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_merge_receiver_t handle, const SacnRecvMergedData* merged_data, void* context)
{
  // Check handle and/or context as necessary...

  // There are two ways to iterate the sources. The first is to iterate each unique active source on the universe:
  for(size_t i = 0; i < merged_data->num_active_sources; ++i)
  {
    SacnMergeReceiverSource source_info;
    etcpal_error_t result = sacn_merge_receiver_get_source(handle, merged_data->active_sources[i], &source_info);

    if(result == kEtcPalErrOk)
    {
      // You wouldn't normally print a message on each sACN update, but this is just for demonstration:
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&source_info.cid, cid_str);

      char ip_str[ETCPAL_IP_STRING_BYTES];
      etcpal_ip_to_string(&source_info.addr.ip, ip_str);

      printf("Slot %u -\n\tCID: %s\n\tName: %s\n\tAddress: %s:%u\n", (merged_data->slot_range.start_address + i), cid_str,
             source_info.name, ip_str, source_info.addr.port);
    }
  }

  // The second way is to iterate the winner/owner of each slot:
  for(unsigned int i = 0; i < merged_data->slot_range.address_count; ++i)
  {
    SacnMergeReceiverSource source_info;
    etcpal_error_t result = sacn_merge_receiver_get_source(handle, merged_data->owners[i], &source_info);
    // ...
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleMergedData(sacn::MergeReceiver::Handle handle, const SacnRecvMergedData& merged_data)
{
  // How to get the merge receiver instance from the handle is application-defined. For example:
  auto merge_receiver = my_app_state.GetMergeReceiver(handle);

  // There are two ways to iterate the sources. The first is to iterate each unique active source on the universe:
  for(size_t i = 0u; i < merged_data.num_active_sources; ++i)
  {
    auto source = merge_receiver.GetSource(merged_data.active_sources[i]);
    if(source)
    {
      // You wouldn't normally print a message on each sACN update, but this is just for demonstration:
      std::cout << "Slot " << (merged_data.slot_range.start_address + i) << " -\n\tCID: " << source->cid.ToString()
                << "\n\tName: " << source->name << "\n\tAddress: " << source->addr.ToString() << "\n";
    }
  }

  // The second way is to iterate the winner/owner of each slot:
  for(unsigned int i = 0; i < merged_data.slot_range.address_count; ++i)
  {
    auto source = merge_receiver.GetSource(merged_data.owners[i]);
    // ...
  }
}
```
<!-- CODE_BLOCK_END -->

## Source Limit Exceeded Conditions

The sACN library will only forward data from sources that it is able to track. When the library is
compilied with #SACN_DYNAMIC_MEM set to 0, this means that it will only track up to
#SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE sources at a time. (You can change these compile options in
your sacn_config.h; see \ref building_and_integrating.)

When the library encounters a source that it does not have room to track, it will send a single
source limit exceeded notification. Additional source limit exceeded callbacks will not be
delivered until the number of tracked sources falls below the limit and then exceeds it again.

If sACN was compiled with #SACN_DYNAMIC_MEM set to 1 (the default on non-embedded platforms), the
library will check against the `source_count_max` value from the merge receiver config/settings,
instead of #SACN_RECEIVER_MAX_SOURCES_PER_UNIVERSE. The `source_count_max` value may be set to
#SACN_RECEIVER_INFINITE_SOURCES, in which case the library will track as many sources as it is
able to dynamically allocate memory for, and this callback will not be called in normal program
operation (in this case it can be set to NULL in the config struct in C).

<!-- CODE_BLOCK_START -->
```c
void my_source_limit_exceeded_callback(sacn_merge_receiver_t handle, uint16_t universe, void* context)
{
  // Check handle and/or context as necessary...

  // Handle the condition in an application-defined way. Maybe log it?
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourceLimitExceeded(sacn::MergeReceiver::Handle handle, uint16_t universe)
{
  // Handle the condition in an application-defined way. Maybe log it?
}
```
<!-- CODE_BLOCK_END -->
