# Changelog

All notable changes to the sACN library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [3.0.0] - 2024-01-12

### Fixed

 - Fixed defects that were causing state corruption when removing a merge receiver, during a
   network reset, and during initialization.
 - Fixed defects that were causing state corruption or crashes when a sACN API call failed.
 - Fixed variations in DMX merger output based on the order levels and priorities were entered.
 - Fixed a defect that was causing sources to be excluded from sampling during lwIP initialization.
 - Fixed incorrect sequence numbers being sent by the source.
 - Fixed incorrect write to reserved field in universe discovery packets.
 - Fixed unsorted universe lists being sent in universe discovery packets.
 - Fixed a defect that was causing redundant multicast sends.
 - Fixed wrapping issues with remote source and merger source handles.
 - Fixed flickering after the sampling period.
 - Removed the const qualifier on the unicast destinations array in the source universe settings.
 - Fixed a couple memory leaks.
 - Audited usage of memory and network resources and fixed a number of issues.

### Added

 - Updated the merge receiver callback interface with additional information and new callbacks.
 - Added priority data to the sacn::MergeReceiver::Source structure.
 - Implemented socket-per-NIC mode on Windows to avoid edge-case performance issues caused by
   the IP_PKTINFO sockopt.
 - Added sacn_config.h options to opt out of SO_REUSEPORT and/or SO_RCVBUF, set the send buffer
   size, and disable internal DMX merger buffers.
 - Changed name of sacn_config.h option to SACN_MERGE_RECEIVER_ENABLE_IN_STATIC_MEMORY_MODE.
 - Implemented a new example application demonstrating use of the sACN Source C++ API.
 - Added ability to specify that no network interfaces should be used, or to specify the same
   interface multiple times.
 - Added new assertion handling (with logging in release mode) throughout the entire codebase.
 - Added logging for send statistics and new error cases, adjusted verbosity of existing logs.

### Changed

 - Revised network reset handling so that new sampling periods will only occur on new interfaces.
 - Improved zero-initialization of internal state memory in static mode.
 - Made refactors to bring in a new version of EtcPal.
 - Adjusted the source implementation to send universes from lowest to highest.
 - Made PAP keep-alive interval separately configurable.
 - Made various performance improvements to the DMX merger.
 - Improved performance when sending many universes by spacing out the sending of start codes 0x00
   and 0xDD.
 - Added handling for more setsockopt error cases.

## [2.0.2] - 2022-09-01

### Fixed

- Fixed numerous defects that were causing static receiver limits (e.g. universe and source counts)
  to be reached sooner than expected.
- Fixed a defect that was causing universe discovery packets with under 40 universes to be
  incorrectly discarded by the source detector.

### Added

- Added a function to the merge receiver for retrieving information about a source on the universe.
- Enabled the installation of PDB files in the lib folder during a CMake install.

## [2.0.1] - 2022-03-02

### Fixed

- Fixed a CMake bug that would cause sACN to fail to configure when EtcPal has already been added
  manually using add_subdirectory()

## 2.0.0 - 2022-02-23

### Added

- New core module APIs: Source, DMX Merger, Merge Receiver, Source Detector
- New C++ wrapper APIs: Source, Receiver, DMX Merger, Merge Receiver, Source Detector

### Changed

- Revamped the API to conform more closely with our other open-source libraries.
- Numerous performance enhancements derived by comprehensive testing in ETC products.

### Removed

## 1.0.0 - 2018-10-18

- Previous existing sACN library - not available on GitHub.

[Unreleased]: https://github.com/ETCLabs/sACN/compare/v3.0.0...main
[3.0.0]: https://github.com/ETCLabs/sACN/compare/v2.0.2...v3.0.0
[2.0.2]: https://github.com/ETCLabs/sACN/compare/v2.0.1...v2.0.2
[2.0.1]: https://github.com/ETCLabs/sACN/compare/v2.0.0...v2.0.1
