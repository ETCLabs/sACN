# Global Initialization and Destruction                                  {#global_init_and_destroy}

The sACN library has overall init and deinit functions that should be called once each at
application startup and shutdown time. These functions interface with the EtcPal \ref etcpal_log
API to configure what happens when the sACN library logs messages. Optionally pass an
EtcPalLogParams structure to use this functionality. This structure can be shared across different
ETC library modules.

<!-- CODE_BLOCK_START -->
```c
// During startup:
EtcPalLogParams log_params = ETCPAL_LOG_PARAMS_INIT;
// Initialize log_params...

sacn_init(&log_params, NULL);
// Or, to init without worrying about logs from the sACN library...
sacn_init(NULL, NULL);

// During shutdown:
sacn_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
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

Individual features can also be initialized separately:

<!-- CODE_BLOCK_START -->
```c
sacn_init_features(NULL, NULL, SACN_FEATURE_DMX_MERGER);

// Later on...
sacn_init_features(NULL, NULL, SACN_FEATURES_ALL_BUT(SACN_FEATURE_DMX_MERGER));

// During shutdown (all featured get deinitialized):
sacn_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
sacn::Init(SACN_FEATURE_DMX_MERGER);

// Later on...
sacn::Init(SACN_FEATURES_ALL_BUT(SACN_FEATURE_DMX_MERGER));

// During shutdown (all featured get deinitialized):
sacn::Deinit();
```
<!-- CODE_BLOCK_END -->
