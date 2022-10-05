# sACN

*Streaming ACN (sACN)* is an ANSI standard for entertainment technology by
[ESTA](http://tsp.esta.org) for transmission of DMX512 data over IP networks. sACN is widely used
in the entertainment industry for real-time control of entertainment technology, especially
lighting fixtures.

This repository contains a C-language library and a C++ wrapper library for communicating via sACN.

## Documentation

For instructions on building the sACN library, as well as an sACN overview and in-depth
documentation, please see the [documentation](https://etclabs.github.io/sACNDocs).

## Supported Platforms

sACN uses EtcPal for platform abstraction.  See [EtcPal's README](https://github.com/ETCLabs/EtcPal#readme) for more information on supported platforms.

## Supported Languages

C++ wrappers support C++ version 14.

C functionality supports C99 with the exception of the following features:

* variable-length arrays
* flexible array members
* designated initializers
* the "restrict" keyword

## Quality Gates

### Code Reviews

* At least 2 developers must approve all code changes made before they can be merged into the integration branch.
* API and major functionality reviews typically include application developers as well.

### Automated Testing

* This consists primarily of unit tests covering the individual API modules.
* Some integration tests have also been made.

### Automated Static Analysis

* Treating warnings as errors is enabled on all platforms.
* Adding Clang Tidy (in phases) is on the TODO list. Once implemented, refer to
.clang-tidy to see which rulesets have been added.

### Automated Style Checking

* Clang format is enabled – currently this follows the style guidelines established for our libraries,
 and it may be updated from time to time. See .clang-format for more details.
* Non-conformance to .clang-format will result in pipeline failures.  The code is not automatically re-formatted.

### Continuous Integration

* A GitLab CI pipeline is being used to run builds and tests that enforce all supported quality gates for all merge
requests, and for generating new library builds from main. See .gitlab-ci.yml for details.

### Automated Dynamic Analysis

* ASAN is currently being used when running all automated tests on Linux to catch various memory errors during runtime.

## Revision Control

sACN development is using Git for revision control.

## License

sACN is licensed under the Apache License 2.0. sACN also incorporates some third-party software
with different license terms, disclosed in ThirdPartySoftware.txt in the directory containing this
README file.

## Standards Version

This library implements ANSI E1.31-2018. You can download the standard document for free from the
[ESTA TSP downloads page](https://tsp.esta.org/tsp/documents/published_docs.php).

## About this ETCLabs Project

sACN is official, open-source software developed by ETC employees and is designed to interact with
ETC products. For challenges using, integrating, compiling, or modifying this software, we
encourage posting on the [issues page](https://github.com/ETCLabs/sACN/issues) of this project.
