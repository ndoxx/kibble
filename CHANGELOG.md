<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/ndoxx/kibble">
    <img src="docs/img/logo.png" alt="Logo" width="80" height="80">
  </a>

<h3 align="center">Kibble::Changelog</h3>
</div>


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