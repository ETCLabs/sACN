# Using the sACN DMX Merger API                                                 {#using_dmx_merger}

The sACN DMX Merger API provides a software merger that takes start code 0 and per-address priority
(PAP) data as input and outputs the merged levels, along with source IDs for each level. This API
exposes both a C and C++ language interface. The C++ interface is a header-only wrapper around the
C interface.

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

sacn_init(&log_params);
// Or, to init without worrying about logs from the sACN library...
sacn_init(NULL);

// During shutdown:
sacn_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "sacn/cpp/dmx_merger.h"

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

A merger must first be created before it can be used. A separate merger should be created for each
universe that needs merging.

To create a merger, a config needs to be initialized and passed into the create/startup function.
In the config, two array pointers must be specified for the merge results: one for the merged slot
levels, and another for the source ID of each slot. The application owns these output arrays and
must ensure that their memory is valid until the merger is destroyed. The maximum source count (the
maximum number of sources the merger will merge when #SACN_DYNAMIC_MEM is 1) is also specified in
the config, and it defaults to #SACN_RECEIVER_INFINITE_SOURCES. If #SACN_DYNAMIC_MEM is 0, then
#SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER is used instead.

Once the merger is created, the merger's functionality can be used. Mergers can also be destroyed
individually with the destroy/shutdown function, although keep in mind that `sacn_deinit()` will
destroy all of the mergers automatically.

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

// Initialize a merger and obtain its handle.
sacn_dmx_merger_t merger_handle;
sacn_dmx_merger_create(&merger_config, &merger_handle);

// Mergers can later be destroyed individually.
// Keep in mind that sacn_deinit() will destroy all mergers automatically.
sacn_dmx_merger_destroy(merger_handle);
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
merger.Startup(settings);

// Mergers can later be destroyed individually.
// Keep in mind that sacn_deinit() will destroy all mergers automatically.
merger.Shutdown();
```
<!-- CODE_BLOCK_END -->

## Adding and Removing Sources

In order to input data into a merger, each source being merged must first be added to the merger.
To do this, use the add source function, which takes the source's CID as input and provides a
source handle on success, or an error code on failure. A source can later be removed individually,
which also removes it from the merge output.

<!-- CODE_BLOCK_START -->
```c
// Add a couple of sources, which are tracked with handles and CIDs.
sacn_source_id_t source_1_handle, source_2_handle;
EtcPalUuid source_1_cid, source_2_cid;
// Initialize CIDs here...

sacn_dmx_merger_add_source(merger_handle, &source_1_cid, &source_1_handle);
sacn_dmx_merger_add_source(merger_handle, &source_2_cid, &source_2_handle);

// Sources can later be removed individually, which updates the merger's output.
sacn_dmx_merger_remove_source(merger_handle, source_1_handle);
sacn_dmx_merger_remove_source(merger_handle, source_2_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Add a couple of sources, which are tracked with handles and CIDs.
etcpal::Uuid source_1_cid, source_2_cid;
// Initialize CIDs here...

// AddSource may also return an error, which is not checked for in this example.
sacn_source_id_t source_1_handle = merger.AddSource(source_1_cid).value();
sacn_source_id_t source_2_handle = merger.AddSource(source_2_cid).value();

// Sources can later be removed individually, which updates the merger's output.
merger.RemoveSource(source_1_handle);
merger.RemoveSource(source_2_handle);
```
<!-- CODE_BLOCK_END -->

## Inputting Levels and Priorities

Different types of data can be fed into a merger: the NULL start code levels, per-address
priorities, and universe priority. Inputting this data into a merger causes its merge output to
synchronously update. There are different ways this data can be passed in, depending on the use
case.

One way to input this data is to pass it in directly, using the update source data function. This
function takes pointers to the NULL start code and per-address priority buffers, their lengths,
and the universe priority value. If the NULL start code or per-address-priority data is not being
updated, the corresponding pointer should be NULL, with a length of 0. Therefore, this function
allows the updating of one or the other, or both. This function would be ideal for initializing
the source data from the receiver API's sources found callback.

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

// Print merge results
for(unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
{
  printf("Slot %u:\n Level: %u\n Source ID: %u\n", i, slots[i], slot_owners[i]);
}
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

// Print merge results
for(unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
{
  std::cout << "Slot " << i << ":\n Level: " << slots[i] << "\n Source ID: " << slot_owners[i] << "\n";
}
```
<!-- CODE_BLOCK_END -->

Another way to input this data is from within the receiver API's universe data callback, using the
update source from sACN function. This function takes the #SacnHeaderData and the pdata, as
provided by the universe data callback. Using these, it determines the universe priority, NULL
start code, and/or per-address priority data, and uses them to do the same update as the update
source data function.

<!-- CODE_BLOCK_START -->
```c
void my_universe_data_callback(sacn_receiver_t handle, uint16_t universe, const EtcPalSockAddr* source_addr, 
                               const SacnHeaderData* header, const uint8_t* pdata, void* context)
{
  // Check handle and/or context as necessary...

  // Look up merger_handle based on universe...

  sacn_dmx_merger_update_source_from_sacn(merger_handle, header, pdata);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleUniverseData(uint16_t universe, const etcpal::SockAddr& source_addr,
                                         const SacnHeaderData& header, const uint8_t* pdata)
{
  // mergers_ is a map between universes and merger objects

  mergers_[universe].UpdateSourceDataFromSacn(header, pdata);
}
```
<!-- CODE_BLOCK_END -->

## Removing Per-Address Priority

A source may stop sending per-address priorities at any time. When this happens, the merger should
reset the priorities for that source to the universe priority and update the merge results
accordingly. This can be done by calling the stop source per-address priority function.

<!-- CODE_BLOCK_START -->
```c
void my_source_pap_lost_callback(sacn_receiver_t handle, uint16_t universe, const SacnRemoteSource* source, 
                                 void* context)
{
  // Check handle and/or context as necessary...

  // Look up merger_handle based on universe...

  // Use the source's CID to look up its handle
  sacn_source_id_t source_handle = sacn_dmx_merger_get_id(merger_handle, &source->cid);

  if(source_handle != SACN_DMX_MERGER_SOURCE_INVALID)
    sacn_dmx_merger_stop_source_per_address_priority(merger_handle, source_handle);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyNotifyHandler::HandleSourcePapLost(uint16_t universe, const SacnRemoteSource& source)
{
  // mergers_ is a map between universes and merger objects

  // Use the source's CID to look up its handle
  auto source_handle = mergers_[universe].GetSourceId(source.cid);

  if(source_handle)
    mergers_[universe].StopSourcePerAddressPriority(*source_handle);
}
```
<!-- CODE_BLOCK_END -->

## Accessing Source Information

Each merger provides read-only access to the state of each of its sources, which can be obtained
by calling the get source function. The state information includes the source's CID, NULL start
code levels, universe priority, and per-address priorities if available.

<!-- CODE_BLOCK_START -->
```c
const SacnDmxMergerSource* source_state = sacn_dmx_merger_get_source(merger_handle, source_1_handle);

if (source_state)
{
  // Do some printouts to demonstrate the fields available
  printf("Source data:\n");

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&source_state->cid, cid_str);
  printf(" CID: %s\n Universe Priority: %u\n", cid_str, source_state->universe_priority);

  for(unsigned int i = 0; i < source_state->valid_value_count; ++i)
  {
    printf(" Level at slot %u: %u\n", i, source_state->values[i]);
  }

  if(source_state->address_priority_valid)
  {
    for(unsigned int i = 0; i < source_state->valid_value_count; ++i)
    {
      printf(" PAP at slot %u: %u\n", i, source_state->address_priority[i]);
    }
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
const SacnDmxMergerSource* source_state = merger.GetSourceInfo(source_1_handle);

if (source_state)
{
  // Do some printouts to demonstrate the fields available
  std::cout << "Source data:\n CID: " << etcpal::Uuid(source_state->cid).ToString() << "\n Universe Priority: " 
            << source_state->universe_priority << "\n";

  for(unsigned int i = 0; i < source_state->valid_value_count; ++i)
  {
    std::cout << " Level at slot " << i << ": " << source_state->values[i] << "\n";
  }

  if(source_state->address_priority_valid)
  {
    for(unsigned int i = 0; i < source_state->valid_value_count; ++i)
    {
      std::cout << " PAP at slot " << i << ": " << source_state->address_priority[i] << "\n";
    }
  }
}
```
<!-- CODE_BLOCK_END -->
