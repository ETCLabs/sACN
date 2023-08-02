# Streaming ACN                                                                         {#mainpage}

## Introduction

sACN is a C-language library (with C++ wrappers) that implements **ANSI E1.31: Lightweight streaming protocol for
transport of DMX512 using ACN**, commonly referred to as **Streaming ACN** or **sACN**. The sACN
library is designed to be portable and scalable to almost any sACN usage scenario, from lightweight
embedded devices to large-scale data sending operations.

Check out \ref getting_started to get started with using the library in your application. To jump
right into the documentation, check out the [Modules Overview](\ref sACN).

## Standard Revision

The sACN library currently implements ANSI E1.31-2018. E1.31-2016 adds synchronization and universe
discovery features to sACN. E1.31-2018 adds IPv6 support.

## Platforms

sACN uses ETC's Platform Abstraction Layer (EtcPal) for platform abstraction, and it runs on every
platform supported by EtcPal; that list can be found [here](https://etclabs.github.io/EtcPalDocs/head/).

## Dependencies

### EtcPal

sACN depends on ETC's Platform Abstraction Library (EtcPal) for platform abstraction. EtcPal is
included automatically in CMake builds using [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake).
This behavior can be overridden in CMake by cloning EtcPal from its
[repository](https://github.com/ETCLabs/EtcPal) and adding its subdirectory before sACN.
