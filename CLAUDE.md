# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A C (C99) and C++ (C++14) library implementing **Streaming ACN (sACN)** — ANSI E1.31-2018, the standard for transmitting DMX512 lighting-control data over IP. It is part of the ETCLabs family of libraries and uses [EtcPal](https://github.com/ETCLabs/EtcPal) for platform abstraction (it runs on full OSes and embedded/no-OS targets).

## Build, test, lint

The library builds with CMake and pulls EtcPal, GoogleTest, fff, etc. via CMake FetchContent (see `cmake/ResolveDependencies.cmake`). Tests, examples, and dev tools are **off by default** — enable them with options.

```sh
mkdir build && cd build
cmake -DSACN_BUILD_TESTS=ON -DSACN_BUILD_EXAMPLES=ON ..   # add -G "Visual Studio 16 2019" -A x64 on Windows
cmake --build . --config Release -j
ctest -C Release --output-on-failure                      # runs the full unit+integration suite
```

Key CMake options: `SACN_BUILD_TESTS`, `SACN_ENABLE_E2E_TESTS` (requires tests), `SACN_BUILD_EXAMPLES`, `SACN_BUILD_TEST_TOOLS`, and the sanitizer toggles `SACN_ENABLE_ASAN/UBSAN/TSAN/MSAN`. CI builds the matrix of these — see `.gitlab-ci.yml`. `SACN_CONFIG_LOC` points at a directory containing a user `sacn_config.h` to override compile-time options.

**Running a single test:** tests are registered with CTest via `gtest_discover_tests`. Use `ctest -R <regex>` to filter, or run the built test executable directly with `--gtest_filter=Suite.Case`. Test result XML lands in `build/tests/test-results/`.

**Lint / format:** clang-format and clang-tidy are quality gates (non-conformance fails CI; code is *not* auto-reformatted). With a configured build tree: `make check_formatting`, and build with `-DSACN_ENABLE_CLANG_TIDY=ON`. clang-tidy rulesets are added in phases — see `.clang-tidy`.

These targets, sanitizer plumbing, and warnings-as-errors only apply when building inside ETC's internal environment (`COMPILING_AS_OSS` is false). A plain OSS checkout uses default config and skips them.

## Architecture

The public C API in `include/sacn/` is mirrored by C++14 wrappers in `include/sacn/cpp/`. The C++ wrappers are header-only adapters over the C API — when changing behavior, change the C layer; the wrapper usually just needs its signatures/docs kept in sync.

There are five functional modules, each present as a C API + C++ wrapper:

- **source** — transmit sACN (act as a console/controller sending DMX universes).
- **receiver** — receive universe data from sources.
- **merge_receiver** — receive *and* merge multiple sources for a universe (priority/HTP), built on top of receiver + dmx_merger.
- **dmx_merger** — standalone per-address priority merge engine.
- **source_detector** — discover sources on the network and the universes they offer (via E1.31 universe discovery).

`common.[ch]` handles library init/deinit and shared types; `pdu.c` packs/parses E1.31 wire packets; `sockets.c` wraps EtcPal multicast networking.

### The three-layer pattern (this is the important part)

Each module's implementation in `src/sacn/` is split into layers that depend strictly downward. Understanding this split is essential before editing:

1. **API layer** — `receiver.c`, `source.c`, `merge_receiver.c`, `dmx_merger.c`, `source_detector.c`. Public entry points: argument validation, locking, and translating between public types and internal state. Thin.
2. **State layer** — `receiver_state.c`, `source_state.c`, `source_detector_state.c`, `source_loss.c`. The protocol and threading logic: tick/processing threads, sampling periods, source-loss/termination tracking, sequencing.
3. **Memory layer** — `mem.c` and the `src/sacn/mem/**` tree (organized by module: `mem/receiver/`, `mem/source/`, `mem/merge_receiver/`, `mem/source_detector/`). Owns every tracked object (receivers, sources, universes, tracked remote sources, status lists, etc.). Each tracked-object type gets its own file.

The memory layer works in **two modes** selected at compile time by `SACN_DYNAMIC_MEM`: dynamic (`malloc`/`free`, default on full OSes) or static fixed-size pools via `etcpal_mempool` (sized by other `opts.h` config). When adding a new tracked structure, you must support both modes — follow the existing pattern in a neighboring `mem/.../*.c` file (allocate/deallocate helpers gated on `SACN_DYNAMIC_MEM`).

All compile-time configuration lives in `include/sacn/opts.h` with documented defaults; users override via `sacn_config.h`.

### Testing architecture (mocks)

`src/sacn_mock/` provides [fff](https://github.com/meekrosoft/fff)-based fake implementations of each module and of the sockets/state layers (e.g. `sacn_mock/receiver.c` fakes the public receiver API; mocks exist for `source_state`, `source_loss`, `sockets`, etc.). This lets a test for one layer mock the layer beneath it and EtcPal (`EtcPalMock`). Tests under `tests/` are organized as:

- `tests/unit/api/{c,cpp}/<module>` — per-module API tests.
- `tests/unit/state/<module>` — state-layer tests.
- `tests/integration/` — cross-layer tests, including `tests/integration/config/<module>_disabled/` which verify the library still compiles with each module compiled out.
- `tests/e2e/` — end-to-end (gated behind `SACN_ENABLE_E2E_TESTS`).

The source file manifest is split into `src/sacn/sources.cmake` and `src/sacn_mock/sources.cmake` (aggregated by `cmake/SacnSourceManifest.cmake`) — **add new source files there**, not by globbing.

## Conventions

- C must compile as C99 **without**: variable-length arrays, flexible array members, designated initializers, or `restrict`. C++ wrappers target C++14.
- Use the `SACN_ASSERT_VERIFY(expr)` macro (from `opts.h`) for internal invariant checks rather than bare `assert`.
- The standard ETC Apache-2.0 license header goes at the top of every source/header file.
