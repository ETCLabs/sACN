# Building and Integrating the sACN Library Into Your Project           {#building_and_integrating}

## Prerequisites

* **CMake >= 3.10**. CMake is an industry-standard cross-platform build system generator for C and
  C++. CMake can be downloaded [here](https://cmake.org/download). It is also available as a
  package in many Linux distributions.

## Configuring sACN

sACN can be provided a configuration file called "sacn_config.h" when compiling to change its
behavior. This is most applicable when compiling sACN for an embedded target; for non-embedded
applications, the default compilation options are usually fine.

To provide an sacn_config.h when building sACN with CMake, pass its location using the
SACN_CONFIG_LOC CMake option:
```
$ cmake -DSACN_CONFIG_LOC=path/to/folder/containing/sacn_config.h ...
```

If you are building sACN manually/without CMake, add the definition `SACN_HAVE_CONFIG_H` in your
compile settings and add the sacn_config.h location to your include paths.

For a list of all possible options that can be included in the sacn_config.h file, see
\ref sacnopts.

### Special Platform Configuration 

Some platforms require special configuration considerations:

* \subpage configuring_lwip

## Including sACN in your project

### Including sACN in CMake projects

To include sACN as a source dependency from a CMake project, use the `add_subdirectory()` command,
specifying the root of the sACN repository, and use `target_link_libraries()` to add the relevant
sACN include paths and binaries to your project settings.

```cmake
add_subdirectory(path/to/sACN/root)
# ...
target_link_libraries(MyApp PRIVATE sACN)
```

### Including sACN in non-CMake projects

sACN can be built on its own using CMake and its headers and binaries can be installed for
inclusion in a non-CMake project. Typical practice is to create a clean directory to hold the build
results named some variation of "build".

**NOTE**: If you are cross-compiling and/or building for an embedded target, some additional
configuration is necessary. EtcPal helps make this possible; see the
[EtcPal embedded build documentation](https://etclabs.github.io/EtcPalDocs/head/building_for_embedded.html)
for more details.

1. Create the build directory:
   ```
   $ mkdir build && cd build
   ```
2. Run CMake to configure the sACN project:
   ```
   $ cmake -DCMAKE_INSTALL_PREFIX=path/to/install/location path/to/sACN/root
   ```
   `-G` can be used to specify a build system; otherwise, CMake will choose a system-appropriate
   default. CMake also has a GUI tool that can be used for this, as well as plugins available for
   several editors and IDEs. `CMAKE_INSTALL_PREFIX` specifies where the final binaries and headers
   will go; if not given, they will be installed in a system appropriate place like
   `/usr/local/include` and `/usr/local/lib` on a *nix system.
3. Use CMake to invoke the generated build system to build the sACN library:
   ```
   $ cmake --build .
   ```
   If you are generating IDE project files, you can use CMake to open the projects in the IDE:
   ```
   $ cmake --open .
   ```
4. Use CMake's installation target to install the built binaries and headers. This usually shows up
   as another project called "INSTALL" inside an IDE or as a target called "install"
   (e.g. `make install` for a makefile generator). You can also do it manually from the command
   line in the build directory:
   ```
   $ cmake -P cmake_install.cmake
   ```
