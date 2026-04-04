<div id="top"></div>
<!--
I'm using the awesome README template from:
https://github.com/othneildrew/Best-README-Template
-->

<!-- PROJECT SHIELDS
see:
https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
<!--[![LinkedIn][linkedin-shield]][linkedin-url]-->



<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/ndoxx/kibble">
    <img src="docs/img/logo.png" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Kibble</h3>

  <p align="center">
    A utility library to jumpstart my projects.
    <br />
    <br />
    <a href="https://github.com/ndoxx/kibble/issues">Report Bug</a>
    ·
    <a href="https://github.com/ndoxx/kibble/issues">Request Feature</a>
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<strong> Table of contents </strong>

- [About the project](#about-the-project)
  - [Kibble](#kibble)
  - [Feature list](#feature-list)
- [Getting started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
  - [Examples and tests](#examples-and-tests)
  - [Documentation](#documentation)
  - [Integration](#integration)
    - [As a git submodule](#as-a-git-submodule)
    - [With CMake's find\_package](#with-cmakes-find_package)
- [Changelog](#changelog)
- [How to help](#how-to-help)
- [License](#license)
- [Contact](#contact)

<!-- ABOUT THE PROJECT -->
# About the project

## Kibble

This C++ library is a collection of utility classes, systems, data structures, algorithms and tools I happen to need quite often in my game development projects. Most of the code is original, some parts stemmed from other open source projects or blog articles I read.<br/>
There are numerous examples on how to use it, and most of the code is unit tested.

At the moment, the code has only been tested under GNU/Linux, and although most of it should compile and work as-is under other operating systems, some OS-dependent code was only implemented for GNU/Linux.<br/>
This library is under heavy development, and the API is subject to change. Use at your own risk!

## Feature list

* Logger
    - Channel-based system with runtime verbosity levels
    - Extendable sink system (out-of-the-box sinks: console, file and TCP)
    - Extendable formatters
    - Extendable policy system for filtering / transforming log data
    - Output is formatted, color coded and easy to follow
    - Formatting syntax inherited from fmtlib
    - Synchronous and asynchronous operation (both thread-safe)
    - Optional stack trace on error
* Program argument parser
    - Automatically generate usage string and version
    - Modern interface
* File system
    - Handles regular files as well as files inside a resource pack
    - Directory / pack aliasing and universal paths
    - Can create and maintain a config directory located where the OS expects it
    - The `kpak` utility can pack a directory easily, it can also be done programmatically
    - Safe file saving utility to avoid overwriting files with bad data
    - MD5 checksum computation
* Serialization
    - Stream serialization helpers
    - Most `std` containers are supported
    - Memory streams
    - Support for versioning and backward compatibility
* Custom assertion
    - Prints a stack trace and can debug break
    - Formatted assertion message strings
* Profiling utility
    - Easily profile execution time
    - Produce a Chrome Tracing json output
* Event system
    - Publish / subscribe model with central message broker (event bus)
    - Deferred event handling with event queues
    - Events can also be fired instantly
    - Abstract delegates allow subscription of free functions as well as member functions
    - Events need not inherit from a base class and can be any type
    - Subscriber priority system to decide which subscribers should respond first
    - Events can be consumed
    - Event tracking / logging
* Job system
    - Launch multiple worker threads and execute tasks asynchronously
    - Lock-free
    - Work stealing scheme for load balancing
    - Dependency graphs
    - Barrier synchronization
    - Worker affinity
    - Recurring tasks
    - Task monitoring and profiling (using the profiling utility)
* Undo engine
    - Qt-like API
    - Push custom commands in a stack-like object that handles the undo / redo logic
    - Manage multiple stacks in an undo group
    - Command merging and compression
    - Clean state
    - Can be wired to an event system thanks to various callbacks
* Random number generation
    - Fast XOR shift engine
    - Coherent noise generation (simplex, octave)
* UUIDv4 generation
    - adapted from: [https://github.com/crashoz/uuid_v4](https://github.com/crashoz/uuid_v4)
* Multiple math utilities
    - Bezier and cubic Hermite spline interpolation (arc-length parameterized)
    - Catenary curve (arc-length parameterized)
    - Constexpr math function collection
    - Easing functions (a lot of them)
    - Separable Gaussian convolution kernel builder
    - Numerical methods (Newton-Raphson, Simpson integration)
    - Statistics class (useful for profiling)
    - Colors: multiple representations (RGBA, HSLA, CIELab, 32b ARGB) and conversion algorithms,
      multiple color difference algorithms...
    - Morton encoding / decoding
* Mathematical optimization and graph algorithms
    - Generic stochastic descent optimizer with FDSA / SPSA support
    - Generic A* search implementation
* Constexpr string hashes and intern string system
    - User defined literal allows to write a full string in the source that will be hashed at compile-time
    - The internstring utility can parse sources and write all string hashes to a text file,
      then it's quite simple to resolve a hash at runtime by reading this file into a map
* Multiple string utilities to avoid boilerplate
    - trimming, stripping, case transform, text centering, tokenization, regex replace
    - Base64 encoding / decoding
* Compile-time type information
    - Drop-in replacement for some features of the CTTI lib
    - Compile-time type name and type ID (as a name hash)
* Memory arena system
    - Fast custom allocators (linear, pool, atomic pool, TLSF)
    - Policy-based memory sanitization
* Basic TCP socket abstraction
    - Simple Berkeley socket wrapper using the Stream / Acceptor / Connector model
* A precision chronometer with a simple interface
* Multiple useful data structures, including sparse sets and sparse pools implementations

<p align="right">(<a href="#top">back to top</a>)</p>


<!-- GETTING STARTED -->
# Getting started

To get a local copy up and running follow these steps.

## Prerequisites

* A compiler with C++23 support (tested with Clang 12+, GCC 13+)
* CMake 3.19 or newer
* Ninja (recommended; used as the default generator)
* [Conan 2](https://conan.io/) package manager - used to provide Catch2 and Google Benchmark for tests and benchmarks

If you intend to run the tests, first install Conan and generate a default profile if you haven't already:
```sh
pip install conan
conan profile detect --force
```

## Installation

1. Clone the repo with its submodules
   ```sh
   git clone --recurse-submodules https://github.com/ndoxx/kibble.git
   cd kibble
   ```

2. Install dependencies with Conan

   The test and benchmark dependencies (Catch2 and Google Benchmark) are managed via Conan. Run the install step for each configuration you intend to build:
   ```sh
   mkdir build && cd build
   conan install .. --output-folder=. --build=missing -s build_type=Debug
   conan install .. --output-folder=. --build=missing -s build_type=Release
   ```

3. Configure with CMake

   Kibble uses the **Ninja Multi-Config** generator, so a single build directory covers both Debug and Release:
   ```sh
   cmake .. \
     -G "Ninja Multi-Config" \
     -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
   ```

4. Build the library
   ```sh
   cmake --build . --config Release --target kibble
   ```

5. Install
   ```sh
   sudo cmake --install . --config Release
   ```

## Examples and tests

Some examples may require additional dependencies like [glm](https://github.com/g-truc/glm).<br/>
Make sure you are in the `build` directory and have run the Conan install step above before building tests or benchmarks.

* Build a single example:
  ```sh
  cmake --build . --config Release --target ex_<example_name>
  ```
  Replace `<example_name>` with the file name stem.

* Build all examples at once:
  ```sh
  cmake --build . --config Release --target kibble_examples
  ```

* Build all unit tests:
  ```sh
  cmake --build . --config Release --target kibble_tests
  ```

* Build all benchmarks:
  ```sh
  cmake --build . --config Release --target kibble_bench
  ```

* You can run all unit tests with:
  ```sh
  ctest -C Release
  ```

> **Note:** tests and benchmarks are excluded from the default build. They are only available when configuring with `-DKB_TARGET_TESTS=ON`, which is the default when building kibble as the top-level project.

<p align="right">(<a href="#top">back to top</a>)</p>

## Documentation

The source files are documented with Doxygen, and higher-level documentation is on its way. To build the docs:
```sh
cmake --build . --config Release --target docs
```

## Integration

### As a git submodule

Kibble can be set up as a subproject with CMake and git. Add it as a submodule, configure it with `set()` directives in your `CMakeLists.txt`, then call `add_subdirectory()`. Kibble's CMake script will detect its use as a subproject, disable the tests, examples and install targets, and let the host project handle output directory configuration.

Note that when used as a submodule, **Conan is not required** - the test and benchmark dependencies are only pulled in when `KB_TARGET_TESTS` is `ON`, which is automatically disabled in subproject mode.

```cmake
set(KB_AREA_MEMORY_INITIALIZATION ON CACHE BOOL "" FORCE)
set(KB_JOB_SYSTEM_PROFILING ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_SOURCE_DIR}/source/vendor/kibble" "external/kibble")

set_target_properties(kibble
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
```

Then link your executable or library against the `kibble` target. That's it.

### With CMake's find_package

During a system installation, Kibble also generates CMake config files so it can be found by CMake:

```cmake
find_package(kibble 1.3.0 REQUIRED)
```


<!-- CHANGELOG -->
# Changelog

See [CHANGELOG.md](CHANGELOG.md).


<!-- CONTRIBUTING -->
# How to help
If you have any idea on how to make this project better, please fork the repo and create a pull request. You can also simply open an issue with the tag "enhancement". Any contribution will be **greatly appreciated**.


1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/FeatureName`)
3. Commit your Changes (`git commit -m 'Add this awesome feature'`)
4. Push to the Branch (`git push origin feature/FeatureName`)
5. Open a Pull Request

If this project was of any help to you, don't hesitate to give it a star!

<p align="right">(<a href="#top">back to top</a>)</p>



<!-- LICENSE -->
# License

Distributed under the MIT License. See `LICENSE.txt` for more information.

<p align="right">(<a href="#top">back to top</a>)</p>



<!-- CONTACT -->
# Contact

Ndoxx - dev.ndoxx@gmail.com

Project Link: [https://github.com/ndoxx/kibble](https://github.com/ndoxx/kibble)

<p align="right">(<a href="#top">back to top</a>)</p>



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/ndoxx/kibble.svg?style=for-the-badge
[contributors-url]: https://github.com/ndoxx/kibble/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/ndoxx/kibble.svg?style=for-the-badge
[forks-url]: https://github.com/ndoxx/kibble/network/members
[stars-shield]: https://img.shields.io/github/stars/ndoxx/kibble.svg?style=for-the-badge
[stars-url]: https://github.com/ndoxx/kibble/stargazers
[issues-shield]: https://img.shields.io/github/issues/ndoxx/kibble.svg?style=for-the-badge
[issues-url]: https://github.com/ndoxx/kibble/issues
[license-shield]: https://img.shields.io/github/license/ndoxx/kibble.svg?style=for-the-badge
[license-url]: https://github.com/ndoxx/kibble/blob/master/LICENSE.txt
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555
[linkedin-url]: https://linkedin.com/in/linkedin_username
[product-screenshot]: images/screenshot.png