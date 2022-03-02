# Changelog

All notable changes to the sACN library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/ETCLabs/sACN/compare/v2.0.1...main
[2.0.1]: https://github.com/ETCLabs/sACN/compare/v2.0.0...v2.0.1
