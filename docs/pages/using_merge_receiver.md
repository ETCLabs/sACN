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
an sacn::MergeReceiver and call its `Startup()` function in C++.

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
config.callbacks.source_limit_exceeded = my_source_limit_exceeded_callback; // optional, can be NULL

sacn_merge_receiver_t my_merge_receiver_handle;
sacn_merge_receiver_create(&config, &my_merge_receiver_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
sacn::MergeReceiver::Settings config(1); // Instantiate config & listen on universe 1
sacn::MergeReceiver merge_receiver; // Instantiate a merge receiver

// You implement the callbacks by implementing the sacn::MergeReceiver::NotifyHandler interface.
// The NotifyHandler-derived instance, referred to here as my_notify_handler, must be passed in to Startup:
MyNotifyHandler my_notify_handler;
merge_receiver.Startup(config, my_notify_handler);
// Or do this if Startup is being called within the NotifyHandler-derived class:
merge_receiver.Startup(config, *this);
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

## Merging

Once a merge receiver has been created, it will begin listening for data on the configured
universe. When DMX level or priority data arrives, or a source is lost, it performs a merge. This
is where it determines the correct DMX level and source for each slot of the universe, since there
may be multiple sources transmitting on the same universe. It does this by selecting the source
with the highest priority. If there are multiple sources at that priority, then it selects the
source with the highest level. That is the Highest Takes Precedence (HTP) merge algorithm.

## Receiving sACN Data

The merged data callback is called whenever there are new merge results for the universe being
listened to, pending the sampling period. The sampling period occurs when a receiver starts
listening on a new universe or a new set of interfaces. Universe data is merged as it comes in
during this period, but the notification of this data doesn't occur until after the sampling period
ends. This removes flicker as various sources in the network are discovered.

This callback will be called in multiple ways:

1. When a new non-preview data packet or per-address priority packet is received from the sACN 
Receiver module, it is immediately and synchronously passed to the DMX Merger. If the sampling
period has not ended, the merged result is not passed to this callback until the sampling period
ends. Otherwise, it is immediately and synchronously passed to this callback.

2. When a sACN source is no longer sending non-preview data or per-address priority packets, the
lost source callback from the sACN Receiver module will be passed to the merger, after which the
merged result is passed to this callback pending the sampling period.

Please note that per-address priority is an ETC-specific sACN extension, and is disabled if the
library is compiled with #SACN_ETC_PRIORITY_EXTENSION set to 0 (in which case per-address priority
packets received have no effect).

The merger will prioritize sources with the highest per-address priority (or universe priority if
the source doesn't provide per-address priorities). If two sources have the same highest priority,
the one with the highest NULL start code level wins (HTP).

There is a key distinction in how the merger interprets the lowest priority. The lowest universe
priority is 0, but the lowest per-address priority is 1. This is because a per-address priority of
0 indicates that the source is not sending any levels to the corresponding slot. Therefore, if
source A has a universe priority of 0 and a level of 10, and source B has a per-address priority of
0 and a level of 50, source A will still win despite having a lower level. This is because
per-address priority 0 indicates that there is no level at this slot at all, whereas universe
priority 0 simply indicates the lowest priority. If source B had a per-address priority of 1, then
source B would win no matter what the levels were.

Also keep in mind that if less than 512 per-address priorities are received, then the remaining
slots will be treated as if they had a per-address priority of 0.

This callback should be processed quickly, since it will interfere with the receipt and processing
of other sACN packets on the universe.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_merge_receiver_t handle, uint16_t universe, const uint8_t* slots, 
                               const sacn_source_id_t* slot_owners, void* context)
{
  // Check handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  printf("Got new merge results on universe %u\n", universe);

  // Example for an sACN-enabled fixture...
  for (int i = 0; i < MY_DMX_FOOTPRINT; ++i)
  {
    if (!SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners, my_start_addr + i))
    {
      // One of the slots in my DMX footprint does not have a valid source
      return;
    }
  }

  memcpy(my_data_buf, &slots[my_start_addr], MY_DMX_FOOTPRINT);
  // Act on the data somehow
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleMergedData(Handle handle, uint16_t universe, const uint8_t* slots,
                                       const sacn_source_id_t* slot_owners)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // fields available:
  std::cout << "Got new merge results on universe " << universe << "\n";

  // Example for an sACN-enabled fixture...
  for (int i = 0; i < MY_DMX_FOOTPRINT; ++i)
  {
    if (!SACN_DMX_MERGER_SOURCE_IS_VALID(slot_owners, my_start_addr + i))
    {
      // One of the slots in my DMX footprint does not have a valid source
      return;
    }
  }

  memcpy(my_data_buf, &slots[my_start_addr], MY_DMX_FOOTPRINT);
  // Act on the data somehow
}
```
<!-- CODE_BLOCK_END -->

If non-NULL start code, non-PAP sACN data is received, it is passed through directly to the
non-DMX data callback.

<!-- CODE_BLOCK_START -->
```c
void my_universe_non_dmx_callback(sacn_merge_receiver_t handle, uint16_t universe, const EtcPalSockAddr* source_addr, 
                                  const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  // Check handle and/or context as necessary...

  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&source_addr->ip, addr_str);

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&header->cid, cid_str);

  printf("Got non-DMX sACN update from source %s (address %s:%u, name %s) on universe %u, priority %u, start code %u\n",
         cid_str, addr_str, source_addr->port, header->source_name, universe, header->priority, header->start_code);

  // Act on the data somehow
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleNonDmxData(Handle handle, uint16_t universe, const etcpal::SockAddr& source_addr,
                                       const SacnHeaderData& header, const uint8_t* pdata)
{
  // You wouldn't normally print a message on each sACN update, but this is just to demonstrate the
  // header fields available:
  std::cout << "Got non-DMX sACN update from source " << etcpal::Uuid(header.cid).ToString() << " (address " 
            << source_addr.ToString() << ", name " << header.source_name << ") on universe " << universe 
            << ", priority " << header.priority << ", start code " << header.start_code << "\n";

  // Act on the data somehow
}
```
<!-- CODE_BLOCK_END -->

## Tracking Sources

The data callbacks include data originating from one or more sources transmitting on the current
universe. Each source has a _Component Identifier_ (CID), which is a UUID that is unique to that
source. The CID should be used as a primary key to differentiate sources. It is provided directly
in the non-DMX callback, in the #SacnHeaderData struct. However, in the merged data callback, only
the source IDs are passed in. To obtain the CID in this case, use the get source CID function.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_merge_receiver_t handle, uint16_t universe, const uint8_t* slots, 
                               const sacn_source_id_t* slot_owners, void* context)
{
  // Check handle and/or context as necessary...

  for(unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    EtcPalUuid cid;
    etcpal_error_t result = sacn_merge_receiver_get_source_cid(handle, slot_owners[i], &cid);

    if(result == kEtcPalErrOk)
    {
      // You wouldn't normally print a message on each sACN update, but this is just for demonstration:
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&cid, cid_str);

      printf("Slot %u CID: %s\n", i, cid_str);
    }
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleMergedData(Handle handle, uint16_t universe, const uint8_t* slots,
                                       const sacn_source_id_t* slot_owners)
{
  for(unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
  {
    // merge_receivers_ is a map between handles and merge receiver objects
    auto cid = merge_receivers_[handle].GetSourceCid(slot_owners[i]);

    if(cid)
    {
      // You wouldn't normally print a message on each sACN update, but this is just for demonstration:
      std::cout << "Slot " << i << " CID: " << cid->ToString() << "\n";
    }
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
void MyNotifyHandler::HandleSourceLimitExceeded(Handle handle, uint16_t universe)
{
  // Handle the condition in an application-defined way. Maybe log it?
}
```
<!-- CODE_BLOCK_END -->
