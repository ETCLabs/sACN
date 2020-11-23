# Streaming ACN                                                                         {#mainpage}

## Introduction

*Streaming ACN (sACN)* is an ANSI standard for entertainment technology by
[ESTA](http://tsp.esta.org) for transmission of DMX512 data over IP networks. sACN is widely used
in the entertainment industry for real-time control of entertainment technology, especially
lighting fixtures.

This repository contains a C-language library and a C++ wrapper library for communicating via sACN.

**NOTE**: This open-source implementation is still in development. The merge receiver, source, and
source detector have not been implemented yet.

Check out \ref getting_started to get started with using the library in your application. To jump
right into the documentation, check out the [Modules Overview](\ref sACN).

## Standard Revision

The sACN library currently implements ANSI E1.31-2018. E1.31-2016 adds synchronization and universe
discovery features to sACN. E1.31-2018 adds IPv6 support (and a requirement for sACN sources to
support IPv6).

## Platforms

sACN uses ETC's Platform Abstraction Layer (EtcPal) for platform abstraction, and it runs on every
platform supported by EtcPal; that list can be found [here](https://etclabs.github.io/EtcPal/docs/head/).

## Dependencies

### EtcPal

sACN depends on ETC's Platform Abstraction Library (EtcPal) for platform abstraction. EtcPal is
included as a git submodule in the sACN repository. The sACN CMake configuration will also
automatically detect the presence of an EtcPal directory at the same level as the sACN root
directory, and use that as its dependency.
