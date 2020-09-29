# Using the sACN DMX Merger API                                                 {#using_dmx_merger}

The sACN DMX Merger API provides a software merger that takes start code 0 and PAP data as input
and outputs the merged levels, along with source IDs for each level. This API exposes both a C and
C++ language interface. The C++ interface is a header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The sACN library has overall init and deinit functions that should be called once each at
application startup and shutdown time. These functions interface with the EtcPal \ref etcpal_log
API to configure what happens when the sACN library logs messages. Optionally pass an
EtcPalLogParams structure to use this functionality. This structure can be shared across different
ETC library modules.

<!-- CODE_BLOCK_START -->
```c
#include "sacn/dmx_merger.h"

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
#include "sacn/cpp/dmx_merger.h"

// During startup:
EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
// Initialize log_params...

etcpal::Error init_result = sacn_init(&log_params);
// Or, to init without worrying about logs from the sACN library...
etcpal::Error init_result = sacn_init(NULL);

// During shutdown:
sacn_deinit();
```
<!-- CODE_BLOCK_END -->

[Merger init description]

<!-- CODE_BLOCK_START -->
```c
// These buffers are updated on each merger call with the merge results.
// They must be valid as long as the merger is using them.
uint8_t slots[DMX_ADDRESS_COUNT];
sacn_source_id_t slot_owners[DMX_ADDRESS_COUNT];

// Merger configuration used for the initialization of each merger:
SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
merger_config.slots = slots;
merger_config.slot_owners = slot_owners;
merger_config.source_count_max = SACN_RECEIVER_INFINITE_SOURCES;

// Initialize a merger and obtain its handle.
sacn_dmx_merger_t merger_handle;
etcpal_error_t result = sacn_dmx_merger_create(&merger_config, &merger_handle);

// Mergers can later be destroyed individually.
// Keep in mind that sacn_deinit() will call this for all mergers automatically.
etcpal_error_t result = sacn_dmx_merger_destroy(merger_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// These buffers are updated on each merger call with the merge results.
// They must be valid as long as the merger is using them.
uint8_t slots[DMX_ADDRESS_COUNT];
sacn_source_id_t slot_owners[DMX_ADDRESS_COUNT];

// Merger configuration used for the initialization of each merger:
sacn::DmxMerger::Settings settings(slots, slot_owners);

// Initialize a merger.
sacn::DmxMerger merger;
etcpal::Error result = merger.Startup(settings);

// Mergers can later be destroyed individually.
// Keep in mind that sacn_deinit() will call this for all mergers automatically.
merger.Shutdown();
```
<!-- CODE_BLOCK_END -->

## Adding and Removing Sources

[description]

<!-- CODE_BLOCK_START -->
```c
// Add a couple of sources, which are tracked with handles and CIDs.
sacn_source_id_t source_1_handle, source_2_handle;
EtcPalUuid source_1_cid, source_2_cid;
// Initialize CIDs here...

etcpal_error_t result;
result = sacn_dmx_merger_add_source(merger_handle, &source_1_cid, &source_1_handle);
result = sacn_dmx_merger_add_source(merger_handle, &source_2_cid, &source_2_handle);

// Sources can later be removed individually.
// Keep in mind that when a merger is destroyed, all of its sources are removed automatically.
result = sacn_dmx_merger_remove_source(merger_handle, source_1_handle);
result = sacn_dmx_merger_remove_source(merger_handle, source_2_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Add a couple of sources, which are tracked with handles and CIDs.
sacn_source_id_t source_1_handle, source_2_handle;
etcpal::Uuid source_1_cid, source_2_cid;
// Initialize CIDs here...

// AddSource may also return an error, which is not checked for in this example.
source_1_handle = merger.AddSource(source_1_cid).value();
source_2_handle = merger.AddSource(source_2_cid).value();

// Sources can later be removed individually.
// Keep in mind that when a merger is destroyed, all of its sources are removed automatically.
merger.RemoveSource(source_1_handle);
merger.RemoveSource(source_2_handle);
```
<!-- CODE_BLOCK_END -->

## Inputting the NULL Start Code and Per-Address Priority Data

[description]

[Pass data directly description]

<!-- CODE_BLOCK_START -->
```c
uint8_t levels[DMX_ADDRESS_COUNT];
uint8_t paps[DMX_ADDRESS_COUNT];
uint8_t universe_priority;
// Initialize levels, paps, and universe_priority here...

// Levels and PAPs can be merged separately:
sacn_dmx_merger_update_source_data(merger_handle, source_1_handle, universe_priority, levels, DMX_ADDRESS_COUNT, NULL,
                                   0);
sacn_dmx_merger_update_source_data(merger_handle, source_1_handle, universe_priority, NULL, 0, paps, DMX_ADDRESS_COUNT);

// Or together in one call:
sacn_dmx_merger_update_source_data(merger_handle, source_2_handle, universe_priority, levels, DMX_ADDRESS_COUNT, paps,
                                   DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_MID -->
```cpp
uint8_t levels[DMX_ADDRESS_COUNT];
uint8_t paps[DMX_ADDRESS_COUNT];
uint8_t universe_priority;
// Initialize levels, paps, and universe_priority here...

// Levels and PAPs can be merged separately:
merger.UpdateSourceData(source_1_handle, universe_priority, levels, DMX_ADDRESS_COUNT);
merger.UpdateSourceData(source_1_handle, universe_priority, nullptr, 0, paps, DMX_ADDRESS_COUNT);

// Or together in one call:
merger.UpdateSourceData(source_2_handle, universe_priority, levels, DMX_ADDRESS_COUNT, paps, DMX_ADDRESS_COUNT);
```
<!-- CODE_BLOCK_END -->

[Within receiver callback description]

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_receiver_t handle, uint16_t universe, const EtcPalSockAddr* source_addr, 
                               const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  // Check handle and/or context as necessary...

  // Find merger_handle...

  sacn_dmx_merger_update_source_from_sacn(merger_handle, header, pdata);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleUniverseData(uint16_t universe, const etcpal::SockAddr& source_addr,
                                         const SacnHeaderData& header, const uint8_t* pdata)
{
  merger_.UpdateSourceDataFromSacn(header, pdata);
}
```
<!-- CODE_BLOCK_END -->

## Removing Per-Address Priority

[description]

<!-- CODE_BLOCK_START -->
```c
void my_source_pap_lost_callback(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, 
                                 void* context)
{
  // Check handle and/or context as necessary...

  // Find merger_handle...

  sacn_source_id_t source_handle = sacn_dmx_merger_get_id(merger_handle, &source->cid);

  if(source_handle != SACN_DMX_MERGER_SOURCE_INVALID)
    etcpal_error_t result = sacn_dmx_merger_stop_source_per_address_priority(merger_handle, source_handle);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourcePapLost(uint16_t universe, const SacnRemoteSource& source)
{
  auto source_handle = merger_.GetSourceId(source.cid);

  if(source_handle)
    merger_.StopSourcePerAddressPriority(*source_handle);
}
```
<!-- CODE_BLOCK_END -->

## Accessing Information for Each Source

[description]

<!-- CODE_BLOCK_START -->
```c
const SacnDmxMergerSource* source_1_state = sacn_dmx_merger_get_source(merger_handle, source_1_handle);
const SacnDmxMergerSource* source_2_state = sacn_dmx_merger_get_source(merger_handle, source_2_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
const SacnDmxMergerSource* source_1_state = merger.GetSourceInfo(source_1_handle);
const SacnDmxMergerSource* source_2_state = merger.GetSourceInfo(source_2_handle);
```
<!-- CODE_BLOCK_END -->
