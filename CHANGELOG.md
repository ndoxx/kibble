<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/ndoxx/kibble">
    <img src="docs/img/logo.png" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Kibble::Changelog</h3>
</div>

# ver 1.3.1
- Base64 utils redesigned and moved to the `base64.h` header
- SHA-1 implementation
- Windows support for TCP socket and bug fixes
- WebSocket support


# ver 1.3.0

- Serialization
  - New `VersionedArchiver` template to enable versioning with stream serializer
  - Fully unit tested


# ver 1.2.5

- Build
  - Moved to C++23
  - Platform and compiler detection header
  - CTTI was made platform independent
  - Builds with GCC
  - Builds on Windows with MSVC (unit tested)
- Various fixes in memory system and event bus
- Fixed clang-tidy `performance` and `cert` warnings
- Runtime CPU instruction sets detection
  - Morton encoding uses a runtime ISA dispatch
- Arbitrary alignment supported by TLSF arena
- Spline class is a bit more generic
- Better stack trace with cpptrace (removed backward-cpp dependency)
- Refactored color header
- Job system:
  - Can detach tasks (Experimental)
  - Fixed a few races
- Assertions can be configured to throw instead of breaking into debugger


# ver 1.2.4

- Modernized `CMake` scripts


# ver 1.2.3

- Refactored pack file system
  - Stream serializer and archiver are used heavily
  - `PackFile`s can be read from generic `std::istream`, in particular `InputMemoryStream`
  - `kpak` utility can export a pack file to a binary array in a C++ header (useful for resource bundling)


# ver 1.2.2

- Added stream serialization helpers and memory streams


# ver 1.2.1

- Removed the old logger, `logger2` was renamed to `logger`


# ver 1.2.0

- Completely removed TOML support and config utility class.