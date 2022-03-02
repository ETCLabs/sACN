# Using the sACN Source API                                                         {#using_source}

The sACN Source API provides a method for applications to transmit sACN data on one or more
universes. This API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The sACN library must be globally initialized before using the Source API. See
\ref global_init_and_destroy.

A source must first be created before it can be used to transmit sACN data. Each source has a name
and a CID. These are specified in the source's configuration structure, along with other optional
settings. Once the source's configuration is initialized, it can simply be passed into the source
Create/Startup function. If the source Create/Startup function succeeds, that source's
functionality will become available. In C, this is accessed with a source handle, whereas in C++
it's accessed through a Source object. A source can later be destroyed using the source Destroy
Shutdown function.

<!-- CODE_BLOCK_START -->
```c
EtcPalUuid my_cid;
char my_name[SACN_SOURCE_NAME_MAX_LEN];
// Assuming my_cid and my_name are initialized by the application...

SacnSourceConfig my_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
my_config.cid = my_cid;
my_config.name = my_name;

sacn_source_t my_handle;
sacn_source_create(&my_config, &my_handle);

// To destroy the source when you're done with it:
sacn_source_destroy(my_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
etcpal::Uuid my_cid;
std::string my_name;
// Assuming my_cid and my_name are initialized by the application...

sacn::Source::Settings my_config(my_cid, my_name);

sacn::Source my_source;
my_source.Startup(my_config);

// To destroy the source when you're done with it:
my_source.Shutdown();
```
<!-- CODE_BLOCK_END -->

## Adding and Removing Universes

A universe represents a destination for the DMX data being transmitted by a source. The receivers
on the other end can choose which universes they want to listen on. Thus, universes allow for the
organization of DMX traffic.

A source can transmit on one or more universes. Each universe has a configuration, which includes
the universe number and other optional settings. Once you create this configuration, you can
simply pass it into the Add Universe function. In this function, you can also specify specific
network interfaces for the source to use for this universe. You can also indicate that all network
interfaces should be used. When you're done transmitting on that universe, you can call the Remove
Universe function.

<!-- CODE_BLOCK_START -->
```c
uint16_t my_universe = 1;  // Using universe 1 as an example.

SacnSourceUniverseConfig my_universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
my_universe_config.universe = my_universe;

SacnMcastInterface my_netints[NUM_MY_NETINTS];
// Assuming my_netints and NUM_MY_NETINTS are initialized by the application...

SacnNetintConfig netint_config;
netint_config.netints = my_netints;
netint_config.num_netints = NUM_MY_NETINTS;

// If you want to specify specific network interfaces to use:
sacn_source_add_universe(my_handle, &my_universe_config, &netint_config);
// Or, if you just want to use all network interfaces:
sacn_source_add_universe(my_handle, &my_universe_config, NULL);
// You can add additional universes as well, in the same way.

// To remove a universe from your source when you're done transmitting on it:
sacn_source_remove_universe(my_handle, my_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
uint16_t my_universe = 1;  // Using universe 1 as an example.

sacn::Source::UniverseSettings my_universe_config(my_universe);

std::vector<SacnMcastInterface> my_netints;
// Assuming my_netints is initialized by the application...

// If you want to specify specific network interfaces to use:
my_source.AddUniverse(my_universe_config, my_netints);
// Or, if you just want to use all network interfaces:
my_source.AddUniverse(my_universe_config);
// You can add additional universes as well, in the same way.

// To remove a universe from your source when you're done transmitting on it:
my_source.RemoveUniverse(my_universe);
```
<!-- CODE_BLOCK_END -->

The application can also obtain the list of universes that a source is currently transmitting on:

<!-- CODE_BLOCK_START -->
```c
uint16_t *universes;  // Points to an array with UNIVERSES_SIZE (perhaps SACN_SOURCE_MAX_UNIVERSES_PER_SOURCE) elements.
size_t num_universes = sacn_source_get_universes(handle_, universes, UNIVERSES_SIZE);
if(num_universes > UNIVERSES_SIZE)
{
  // Not all of the universes were written to the array. The application should allocate a larger array and try again.
}
else
{
  // The first num_universes elements of the universes array represent the complete list of universes.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
std::vector<uint16_t> universes = my_source.GetUniverses();
```
<!-- CODE_BLOCK_END -->

## Update Levels and Per-Address Priorities

At this point, once you have new data to transmit, you can use one of the Update functions to
copy in the NULL start code or per-address priority (PAP) data that should be transmitted on the
network. Assuming you didn't set the manually_process_source setting to true, the source thread
will take care of actually sending the data. Otherwise, you'll need to call the Process Manual
function at your DMX rate (typically 23 ms).

Please note that per-address priority is an ETC-specific sACN extension, and is disabled if the
library is compiled with #SACN_ETC_PRIORITY_EXTENSION set to 0.

<!-- CODE_BLOCK_START -->
```c
// Initialize my_levels_buffer and (possibly) my_priorities_buffer with the data you want to send...
uint8_t my_levels_buffer[DMX_ADDRESS_COUNT];
uint8_t my_priorities_buffer[DMX_ADDRESS_COUNT];

// Now copy in the new data to send. Then the source thread will handle transmitting the 
// data (unless you set manually_process_source to true in the SacnSourceConfig).
sacn_source_update_levels(my_handle, my_universe, my_levels_buffer, DMX_ADDRESS_COUNT);
// Or if you're using per-address priorities:
sacn_source_update_levels_and_pap(my_handle, my_universe, my_levels_buffer, DMX_ADDRESS_COUNT, my_priorities_buffer, DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Initialize my_levels_buffer and (possibly) my_priorities_buffer with the data you want to send...
uint8_t my_levels_buffer[DMX_ADDRESS_COUNT];
uint8_t my_priorities_buffer[DMX_ADDRESS_COUNT];

// Now copy in the new data to send. Then the source thread will handle transmitting the 
// data (unless you set manually_process_source to true in the Source::Settings).
my_source.UpdateLevels(my_universe, my_levels_buffer, DMX_ADDRESS_COUNT);
// Or if you're using per-address priorities:
my_source.UpdateLevelsAndPap(my_universe, my_levels_buffer, DMX_ADDRESS_COUNT, my_priorities_buffer, DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_END -->

## Multicast and Unicast

By default, sources transmit DMX data over multicast. The destination multicast address is based on
the universe number, therefore each universe's data goes to a different multicast address. However,
sources can also transmit to one or more unicast addresses in addition to multicast. To add a
unicast destination for a universe, call the Add Unicast Destination function. There's also a
Remove Unicast Destination function. The universe configuration also has a send_unicast_only
setting, which disables the transmission of multicast altogether. Once you change the unicast
configuration, transmission suppression is reset and the newly added unicast destinations will have
data transmitted to them.

<!-- CODE_BLOCK_START -->
```c
// Unicast can be sent to one or more addresses, in addition to multicast.
EtcPalIpAddr custom_destination;  // Application initializes custom_destination...
sacn_source_add_unicast_destination(my_handle, my_universe, &custom_destination);

// You can remove a unicast destination previously added:
sacn_source_remove_unicast_destination(my_handle, my_universe, &custom_destination);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Unicast can be sent to one or more addresses, in addition to multicast.
etcpal::IpAddr custom_destination;  // Application initializes custom_destination...
my_source.AddUnicastDestination(my_universe, custom_destination);

// You can remove a unicast destination previously added:
my_source.RemoveUnicastDestination(my_universe, custom_destination);
```
<!-- CODE_BLOCK_END -->

The starting set of unicast destinations can also be specified with the universe configuration's
optional unicast_destinations setting.

The application can also obtain the list of unicast destinations that a universe is currently
transmitting on:

<!-- CODE_BLOCK_START -->
```c
EtcPalIpAddr *unicast_dests;  // Points to an array with DESTS_SIZE (perhaps SACN_MAX_UNICAST_DESTINATIONS) elements.
size_t num_dests = sacn_source_get_unicast_destinations(handle_, my_universe, unicast_dests, DESTS_SIZE);
if(num_dests > DESTS_SIZE)
{
  // Not all of the destinations were written to the array. The application should allocate a larger array & try again.
}
else
{
  // The first num_dests elements of the unicast_dests array represent the complete list of unicast destinations.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
std::vector<etcpal::IpAddr> unicast_dests = my_source.GetUnicastDestinations(my_universe);
```
<!-- CODE_BLOCK_END -->


## Custom Start Codes

The Update functions only allow you to send start code 0x00 (NULL) and 0xDD (PAP) data. If
you want to send data for a different start code, you'll need to use the Send Now function to
transmit that data synchronously.

<!-- CODE_BLOCK_START -->
```c
uint8_t my_custom_start_code;
uint8_t my_custom_start_code_data[DMX_ADDRESS_COUNT];
// Initialize start code and data...

sacn_source_send_now(my_handle, my_universe, my_custom_start_code, my_custom_start_code_data, DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_MID -->
```cpp
uint8_t my_custom_start_code;
uint8_t my_custom_start_code_data[DMX_ADDRESS_COUNT];
// Initialize start code and data...

my_source.SendNow(my_universe, my_custom_start_code, my_custom_start_code_data, DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_END -->

## sACN Sync

You can also configure synchronization universes for each of your universes using the Change
Synchronization Universe function. Then the transmitted DMX data will include this synchronization
universe, indicating to the receivers to wait to apply the data until a synchronization message is
received on the specified synchronization universe. To send the synchronization message, call the
Send Synchronization function. NOTE: sACN Sync will not be supported in sACN 2.0.x.

<!-- CODE_BLOCK_START -->
```c
// You can also set up a synchronization universe for a universe.
// Receivers should hang on to the data and wait for a sync message.
uint16_t my_sync_universe = 123;  // Let's say the sync universe is 123, for example.
sacn_source_change_synchronization_universe(my_handle, my_universe, my_sync_universe);

// Whenever you want the data to be applied, you can immediately send a sync message for your universe.
sacn_source_send_synchronization(my_handle, my_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
// You can also set up a synchronization universe for a universe.
// Receivers should hang on to the data and wait for a sync message.
uint16_t my_sync_universe = 123;  // Let's say the sync universe is 123, for example.
my_source.ChangeSynchronizationUniverse(my_universe, my_sync_universe);

// Whenever you want the data to be applied, you can immediately send a sync message for your universe.
my_source.SendSynchronization(my_universe);
```
<!-- CODE_BLOCK_END -->

The initial setting for sync universe can also be specified with the universe configuration's
optional sync_universe setting.

## Changing Preview Flag, Universe Priority, or Source Name

The preview flag, universe priority, and/or source name can be changed at any time using the
appropriate Change function. This will also update the data being sent on the network and reset
transmission suppression.

<!-- CODE_BLOCK_START -->
```c
// The preview flag, priority, and name can also be changed at any time:
const char* new_name = "Hello World";
sacn_source_change_name(my_handle, new_name)
uint8_t new_priority = 50;
sacn_source_change_priority(my_handle, my_universe, new_priority);
bool new_preview_flag = true;
sacn_source_change_preview_flag(my_handle, my_universe, new_preview_flag);
```
<!-- CODE_BLOCK_MID -->
```cpp
// The preview flag, priority, and name can also be changed at any time:
std::string new_name("Hello World");
my_source.ChangeName(new_name)
uint8_t new_priority = 50;
my_source.ChangePriority(my_universe, new_priority);
bool new_preview_flag = true;
my_source.ChangePreviewFlag(my_universe, new_preview_flag);
```
<!-- CODE_BLOCK_END -->
