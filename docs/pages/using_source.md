# Using the sACN Source API                                                         {#using_source}

The sACN Source API provides a method for applications to transmit sACN data on one or more
universes. This API exposes both a C and C++ language interface. The C++ interface is a
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
#include "sacn/source.h"

// During startup:
EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
// Initialize log_params...

sacn_init(&log_params);
// Or, to init without worrying about logs from the sACN library...
sacn_init(NULL);

// During shutdown:
sacn_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "sacn/cpp/source.h"

// During startup:
etcpal::Logger logger;
// Initialize logger...

sacn::Init(logger);
// Or, to init without worrying about logs from the sACN library...
sacn::Init();

// During shutdown:
sacn::Deinit();
```
<!-- CODE_BLOCK_END -->

A source must first be created before it can be used to transmit sACN data. Each source has a name
and a CID. These are specified in the source's configuration structure, along with other optional
settings. Once the source's configuration is initialized, it can simply be passed into the source
Create/Startup function. In this function, you can also specify specific network interfaces for the
source to use. You can also indicate that all network interfaces should be used. If the source
Create/Startup function succeeds, that source's functionality will become available. In C, this is
accessed with a source handle, whereas in C++ it's accessed through a Source object. A source can
later be destroyed using the source Destroy/Shutdown function.

<!-- CODE_BLOCK_START -->
```c
EtcPalUuid my_cid;
char my_name[SACN_SOURCE_NAME_MAX_LEN];
// Assuming my_cid and my_name are initialized by the application...

SacnSourceConfig my_config = SACN_SOURCE_CONFIG_DEFAULT_INIT;
sacn_source_config_init(&my_config, &my_cid, my_name);

SacnMcastInterface my_netints[NUM_MY_NETINTS];
// Assuming my_netints and NUM_MY_NETINTS are initialized by the application...

sacn_source_t my_handle;

// If you want to specify specific network interfaces to use:
sacn_source_create(&my_config, &my_handle, my_netints, NUM_MY_NETINTS);
// Or, if you just want to use all network interfaces:
sacn_source_create(&my_config, &my_handle, NULL, 0);

// To destroy the source when you're done with it:
sacn_source_destroy(my_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
etcpal::Uuid my_cid;
std::string my_name;
// Assuming my_cid and my_name are initialized by the application...

Settings my_config(my_cid, my_name);

std::vector<SacnMcastInterface> my_netints;
// Assuming my_netints is initialized by the application...

Source my_source;

// If you want to specify specific network interfaces to use:
my_source.Startup(my_config, my_netints);
// Or, if you just want to use all network interfaces:
my_source.Startup(my_config, std::vector<SacnMcastInterface>());

// To destroy the source when you're done with it:
my_source.Shutdown();
```
<!-- CODE_BLOCK_END -->

## Adding and Removing Universes

A universe represents a destination for the DMX data being transmitted by a source. The receivers
on the other end can choose which universes they want to listen on. Thus, universes allow for the
organization of DMX traffic.

A source can transmit on one or more universes. Each universe has a configuration, which includes
the universe number, values buffer, and optionally the per-address priority buffer, in addition to
other optional settings. The buffers you pass into the configuration contain the data you want to
send for those start codes (0x00 for values, 0xDD for PAP). Once you create this configuration, you
can simply pass it into the Add Universe function. When you're done transmitting on that universe,
you can call the Remove Universe function.

<!-- CODE_BLOCK_START -->
```c
uint8_t my_values_buffer[DMX_ADDRESS_COUNT];
uint8_t my_priorities_buffer[DMX_ADDRESS_COUNT];

uint16_t my_universe = 1;  // Using universe 1 as an example.

SacnSourceUniverseConfig my_universe_config = SACN_SOURCE_UNIVERSE_CONFIG_DEFAULT_INIT;
sacn_source_universe_config_init(&my_universe_config, my_universe, my_values_buffer, DMX_ADDRESS_COUNT,
                                 my_priorities_buffer);
// Or, if you don't want to send per-address priorities:
sacn_source_universe_config_init(&my_universe_config, my_universe, my_values_buffer, DMX_ADDRESS_COUNT, NULL);
my_universe_config.priority = 123;

sacn_source_add_universe(my_handle, &my_universe_config);
// You can add additional universes as well, in the same way.

// To remove a universe from your source when you're done transmitting on it:
sacn_source_remove_universe(my_handle, my_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
uint8_t my_values_buffer[DMX_ADDRESS_COUNT];
uint8_t my_priorities_buffer[DMX_ADDRESS_COUNT];

uint16_t my_universe = 1;  // Using universe 1 as an example.

UniverseSettings my_universe_config(my_universe, my_values_buffer, DMX_ADDRESS_COUNT);
// If you want per-address priorities:
my_universe_config.priorities_buffer = my_priorities_buffer;
// Otherwise, specify a universe priority:
my_universe_config.priority = 123;

my_source.AddUniverse(my_universe_config);
// You can add additional universes as well, in the same way.

// To remove a universe from your source when you're done transmitting on it:
my_source.RemoveUniverse(my_universe);
```
<!-- CODE_BLOCK_END -->

## Set Dirty

At this point, once you have the desired data in your configured buffer(s), you can use the Set
Dirty function to indicate that the data should be transmitted on the network. Assuming you didn't
set the manually_process_source setting to true, the source thread will take care of actually
sending the data. Otherwise, you'll need to call the Process All function at your DMX rate
(typically 23 ms). Ultimately, you should call Set Dirty whenever the data in your buffer(s)
changes and you want that change transmitted to the network.

<!-- CODE_BLOCK_START -->
```c
// Initialize my_values_buffer and (possibly) my_priorities_buffer with the values you want to send...

// Now set the universe to dirty. Then the source thread will handle transmitting the data (unless you set
// manually_process_source to true in the SacnSourceConfig).
sacn_source_set_dirty(my_handle, my_universe);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Initialize my_values_buffer and (possibly) my_priorities_buffer with the values you want to send...

// Now set the universe to dirty. Then the source thread will handle transmitting the data (unless you set
// manually_process_source to true in the Source::Settings).
my_source.SetDirty(my_universe);
```
<!-- CODE_BLOCK_END -->

## Multicast and Unicast

By default, sources transmit DMX data over multicast. The destination multicast address is based on
the universe number, therefore each universe's data goes to a different multicast address. However,
sources can also transmit to one or more unicast addresses in addition to multicast. To add a
unicast destination for a universe, call the Add Unicast Destination function. There's also a
Remove Unicast Destination function. The universe configuration also has a send_unicast_only
setting, which disables the transmission of multicast altogether. Once you change the unicast
configuration, call Set Dirty to transmit using that new configuration.

<!-- CODE_BLOCK_START -->
```c
// Unicast can be sent to one or more addresses, in addition to multicast.
EtcPalIpAddr custom_destination;  // Application initializes custom_destination...
sacn_source_add_unicast_destination(my_handle, my_universe, &custom_destination);
sacn_source_set_dirty(my_handle, my_universe); // Indicate the data should be sent on multicast and unicast (or just
                                               // unicast if send_unicast_only is enabled in SacnSourceUniverseConfig).

// You can remove a unicast destination previously added:
sacn_source_remove_unicast_destination(my_handle, my_universe, &custom_destination);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Unicast can be sent to one or more addresses, in addition to multicast.
etcpal::IpAddr custom_destination;  // Application initializes custom_destination...
my_source.AddUnicastDestination(my_universe, custom_destination);
my_source.SetDirty(my_universe); // Indicate the data should be sent on multicast and unicast (or just unicast if
                                 // send_unicast_only is enabled in Source::UniverseSettings).

// You can remove a unicast destination previously added:
my_source.RemoveUnicastDestination(my_universe, custom_destination);
```
<!-- CODE_BLOCK_END -->

## Custom Start Codes

The buffers in the universe configuration only allow you to send start code 0x00 (NULL) and 0xDD
(PAP) data. If you want to send data for a different start code, you'll need to use the Send Now
function to transmit that data synchronously.

<!-- CODE_BLOCK_START -->
```c
// Custom start code data can also be sent immediately:
uint8_t my_custom_start_code;
uint8_t my_custom_start_code_data[DMX_ADDRESS_COUNT];
// Initialize start code and data...
sacn_source_send_now(my_handle, my_universe, my_custom_start_code, my_custom_start_code_data, DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Custom start code data can also be sent immediately:
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
Send Synchronization function.

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

## Changing Preview Flag, Universe Priority, or Source Name

The preview flag, universe priority, and/or source name can be changed at any time using the
appropriate Change function, followed by a call to Set Dirty to transmit the changes to the network.

<!-- CODE_BLOCK_START -->
```c
// The preview flag, priority, and name can also be changed at any time:
const char* new_name = "Hello World";
sacn_source_change_name(my_handle, new_name)
uint8_t new_priority = 50;
sacn_source_change_priority(my_handle, my_universe, new_priority);
bool new_preview_flag = true;
sacn_source_change_preview_flag(my_handle, my_universe, new_preview_flag);
sacn_source_set_dirty(my_handle, my_universe);  // Indicate new data should be transmitted.
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
my_source.SetDirty(my_universe);  // Indicate new data should be transmitted.
```
<!-- CODE_BLOCK_END -->
