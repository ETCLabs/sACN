# Using the sACN DMX Merger API                                                 {#using_dmx_merger}

The sACN DMX Merger API provides a software merger that takes level (NULL start code) and priority
data as input and outputs the merged levels, along with source IDs and per-address priorities
(PAP) for each merged level. This API exposes both a C and C++ language interface. The C++
interface is a header-only wrapper around the C interface.

Please note that per-address priority is an ETC-specific sACN extension, and is disabled if the
library is compiled with #SACN_ETC_PRIORITY_EXTENSION set to 0.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The sACN library must be globally initialized before using the DMX Merger API. See
\ref global_init_and_destroy.

A merger must first be created before it can be used. A separate merger should be created for each
universe that needs merging.

To create a merger, a config needs to be initialized and passed into the create/startup function.
In the config, the output array pointer for the merged slot levels must be specified. There are
also optional outputs: the source IDs of each slot, the per-address priorities of each slot,
whether per-address priority packets should be transmitted to sACN, and the universe priority to
transmit. These are set to NULL if not used. The application owns the memory that it points to in
the config, and must ensure that the memory provided is valid until the merger is destroyed. The
maximum source count (the maximum number of sources the merger will merge when #SACN_DYNAMIC_MEM
is 1) is also specified in the config, and it defaults to #SACN_RECEIVER_INFINITE_SOURCES. If
#SACN_DYNAMIC_MEM is 0, then #SACN_DMX_MERGER_MAX_SOURCES_PER_MERGER is used instead.

Once the merger is created, the merger's functionality can be used. Mergers can also be destroyed
individually with the destroy/shutdown function, although keep in mind that all mergers are
destroyed automatically when sACN deinitializes.

<!-- CODE_BLOCK_START -->
```c
// These buffers are updated on each merger call with the merge results.
uint8_t merged_levels[DMX_ADDRESS_COUNT];
uint8_t per_address_priorities[DMX_ADDRESS_COUNT];
bool should_transmit_per_address_priorities = false;
uint8_t universe_priority_to_transmit = 0;
sacn_dmx_merger_source_t owners[DMX_ADDRESS_COUNT];

// Merger configuration used for the initialization of each merger:
SacnDmxMergerConfig merger_config = SACN_DMX_MERGER_CONFIG_INIT;
merger_config.levels = merged_levels;
merger_config.per_address_priorities = per_address_priorities;
merger_config.per_address_priorities_active = &should_transmit_per_address_priorities;
merger_config.universe_priority = &universe_priority_to_transmit;
merger_config.owners = owners;

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
uint8_t merged_levels[DMX_ADDRESS_COUNT];
uint8_t per_address_priorities[DMX_ADDRESS_COUNT];
bool should_transmit_per_address_priorities = false;
uint8_t universe_priority_to_transmit = 0;
sacn_dmx_merger_source_t owners[DMX_ADDRESS_COUNT];

// Merger configuration used for the initialization of each merger:
sacn::DmxMerger::Settings settings(merged_levels);
settings.per_address_priorities = per_address_priorities;
settings.per_address_priorities_active = &should_transmit_per_address_priorities;
settings.universe_priority = &universe_priority_to_transmit;
settings.owners = owners;

// Initialize a merger.
sacn::DmxMerger merger;
merger.Startup(settings);

// Mergers can later be destroyed individually.
// Keep in mind that sacn::Deinit() will destroy all mergers automatically.
merger.Shutdown();
```
<!-- CODE_BLOCK_END -->

## Adding and Removing Sources

In order to input data into a merger, each source being merged must first be added to the merger.
To do this, use the add source function, which provides a new source handle on success, or an error
code on failure. A source can later be removed individually, which also removes it from the merge
output.

<!-- CODE_BLOCK_START -->
```c
// Add a couple of sources, which are tracked with handles.
sacn_dmx_merger_source_t source_1_handle, source_2_handle;

sacn_dmx_merger_add_source(merger_handle, &source_1_handle);
sacn_dmx_merger_add_source(merger_handle, &source_2_handle);

// Sources can later be removed individually, which updates the merger's output.
sacn_dmx_merger_remove_source(merger_handle, source_1_handle);
sacn_dmx_merger_remove_source(merger_handle, source_2_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Add a couple of sources, which are tracked with handles.
// AddSource may also return an error, which is not checked for in this example.
sacn_dmx_merger_source_t source_1_handle = merger.AddSource().value();
sacn_dmx_merger_source_t source_2_handle = merger.AddSource().value();

// Sources can later be removed individually, which updates the merger's output.
merger.RemoveSource(source_1_handle);
merger.RemoveSource(source_2_handle);
```
<!-- CODE_BLOCK_END -->

## Inputting Levels and Priorities

Different types of data can be fed into a merger: the NULL start code levels, per-address
priorities, and universe priority. Inputting this data into a merger causes its merge output to
synchronously update. The merger will prioritize sources with the highest per-address priority
(or universe priority if the source doesn't provide per-address priorities). If two sources have
the same highest priority, the one with the highest NULL start code level wins (HTP). For each
slot, a source can only be included in the merge if it has both a level and a priority at that
slot. Otherwise, it will be as if the source were not sourcing that slot.

There is a key distinction in how the merger interprets the lowest priority. The lowest universe
priority is 0, but the lowest per-address priority is 1. This is because a per-address priority of
0 indicates that the source is not sending any levels to the corresponding slot. The solution the
merger uses is to always track priorities per-address. If a source only has a universe priority,
that priority is used for each slot, except if it equals 0 - in that case, it is converted to 1.
This means the merger will treat a universe priority of 0 as equivalent to a universe priority of
1, as well as per-address priorities equal to 1.

The merger may output per-address priorities if configured to do so by initializing
per_address_priorities in the merger config/settings. If a universe priority of 1 or above wins a
slot, the merger will output the same value for the per-address priority for that slot. However, if
a universe priority of 0 wins a slot, the merger will output a per-address priority of 1 for that
slot. If a per-address priority wins, that same value is used for the per-address priority output.
The merger only outputs a per-address priority of 0 if there is no winner for that slot.

Also keep in mind that if less than 512 per-address priorities are inputted, then the remaining
slots will be treated as if they had a per-address priority of 0. Likewise, if less than 512 levels
are inputted, the remaining slots will be treated as if they had a level of 0, but they may still
have non-zero priorities.

The way to input this data for a source is to pass it in directly, using one of the update
functions. The application can use these functions to update the levels, universe priority, and/or
per-address priorities (PAP) individually. The source won't be factored into the merge output until
one of the update priority functions are called. If the update levels function hasn't been called
at this point, and the source still wins a slot, then the merger will output a level of 0. 

<!-- CODE_BLOCK_START -->
```c
uint8_t levels[DMX_ADDRESS_COUNT];
uint8_t pap[DMX_ADDRESS_COUNT];
uint8_t universe_priority;
// Initialize levels, pap, and universe_priority here...

// To update levels:
sacn_dmx_merger_update_levels(merger_handle, source_1_handle, levels, DMX_ADDRESS_COUNT);

// Then call one of these to update priority:
sacn_dmx_merger_update_universe_priority(merger_handle, source_1_handle, universe_priority);
sacn_dmx_merger_update_pap(merger_handle, source_1_handle, pap, DMX_ADDRESS_COUNT);

// Now the source has been factored into the merge results, which are printed here.
for(unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
{
  printf("Slot %u:\n Level: %u\n PAP: %u\n Source ID: %u\n", i, merged_levels[i], per_address_priorities[i], owners[i]);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
uint8_t levels[DMX_ADDRESS_COUNT];
uint8_t pap[DMX_ADDRESS_COUNT];
uint8_t universe_priority;
// Initialize levels, pap, and universe_priority here...

// To update levels:
merger.UpdateLevels(source_1_handle, levels, DMX_ADDRESS_COUNT);

// Then call one of these to update priority:
merger.UpdateUniversePriority(source_1_handle, universe_priority);
merger.UpdatePap(source_1_handle, pap, DMX_ADDRESS_COUNT);

// Now the source has been factored into the merge results, which are printed here.
for(unsigned int i = 0; i < DMX_ADDRESS_COUNT; ++i)
{
  std::cout << "Slot " << i << ":\n Level: " << merged_levels[i] << ":\n PAP: " << per_address_priorities[i] << "\n Source ID: "
            << owners[i] << "\n";
}
```
<!-- CODE_BLOCK_END -->

## Accessing Source Information

Each merger provides read-only access to the state of each of its sources, which can be obtained
by calling the get source function. The state information includes the source's handle, NULL start
code levels, universe priority, and per-address priorities if available.

<!-- CODE_BLOCK_START -->
```c
const SacnDmxMergerSource* source_state = sacn_dmx_merger_get_source(merger_handle, source_1_handle);

if (source_state)
{
  // Do some printouts to demonstrate the fields available
  printf("Source data:\n");

  printf(" Universe Priority: %u\n", source_state->universe_priority);

  for(unsigned int i = 0; i < source_state->valid_level_count; ++i)
  {
    printf(" Level at slot %u: %u\n", i, source_state->levels[i]);
  }

  if(source_state->address_priority_valid)
  {
    for(unsigned int i = 0; i < source_state->valid_level_count; ++i)
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
  std::cout << "Source data:\n Universe Priority: " << source_state->universe_priority << "\n";

  for(unsigned int i = 0; i < source_state->valid_level_count; ++i)
  {
    std::cout << " Level at slot " << i << ": " << source_state->levels[i] << "\n";
  }

  if(source_state->address_priority_valid)
  {
    for(unsigned int i = 0; i < source_state->valid_level_count; ++i)
    {
      std::cout << " PAP at slot " << i << ": " << source_state->address_priority[i] << "\n";
    }
  }
}
```
<!-- CODE_BLOCK_END -->
