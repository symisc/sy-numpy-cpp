# syNumpy

`syNumpy` is a standalone **C++17** library from **Symisc Systems** for reading and writing NumPy `.npy` files.

Project homepage and documentation: [https://pixlab.io/numpy-cpp-library](https://pixlab.io/numpy-cpp-library)

## Production Usage

`syNumpy` is used internally by Symisc/PixLab production systems.

- It is used by [FACEIO](https://faceio.net/) for internal facial features extraction workflows.
- It is used by PixLab document and visual search workloads behind the [DocScan Mobile App](https://pixlab.io/mobile-apps/docscan-app) and StreetLens mobile app integrations built on top of the [PixLab API Endpoints](https://pixlab.io/api-endpoints) ecosystem.
- It also integrates naturally with the [DOCSCAN ID Scan API](https://pixlab.io/id-scan-api/docscan), which is designed to scan and extract structured data from identity documents worldwide.

## Highlights

- Single public header and single implementation file: `synumpy.hpp` and `synumpy.cpp`
- C++17 API with no external runtime dependency
- Core parser centered on `syNumpy::loadNpyBuffer()`
- Load `.npy` arrays from disk or directly from a memory buffer
- Save typed vectors and raw buffers to `.npy`
- Append mode for extending the first dimension of an existing array
- Strict validation for malformed headers, truncated payloads, and unsupported dtype/layout combinations

## Scope Of This Release

This public release focuses on robust `.npy` support.

- Supported: scalar numeric and complex NumPy dtypes that map cleanly to native C++ types
- Supported: shape metadata, payload loading, file loading, memory-buffer loading, and append-on-axis-0
- Supported: explicit dtype-based raw save through `syNumpy::saveNpyRaw()`
- Not implemented in this release: `.npz` archive support
- Endianness is validated explicitly; cross-endian payloads are rejected rather than silently byte-swapped

## Repository Layout

- `synumpy.hpp`: public C++ API
- `synumpy.cpp`: implementation
- `example.cpp`: minimal usage sample
- `CMakeLists.txt`: CMake build integration
- `Makefile`: simple Unix-style build integration

## Build & Integration

The easiest way to integrate `syNumpy` is to drop `synumpy.hpp` and `synumpy.cpp` directly into your codebase and compile them as part of your project. No complex build scripts, generators, or external runtime dependencies are required.

### CMake

```bash
cmake -S . -B build
cmake --build build --config Release
```

To install:

```bash
cmake --install build --config Release
```

### Make

```bash
make
```

This builds:

- `build/libsynumpy.a`
- `build/example`

To install:

```bash
make install
```

### Direct Compilation

MSVC:

```bat
cl /std:c++17 /EHsc /W4 synumpy.cpp example.cpp
```

GCC or Clang:

```bash
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic synumpy.cpp example.cpp -o example
```

## Quick Start

Include the public header:

```cpp
#include "synumpy.hpp"
```

The main entry points are:

- `syNumpy::NpyArray`
- `syNumpy::loadNpyBuffer()`
- `syNumpy::loadNpy()`
- `syNumpy::saveNpy()`
- `syNumpy::saveNpyRaw()`

## Minimal Example

```cpp
#include "synumpy.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

int main() {
    const std::vector<float> values = {1.0f, 2.0f, 3.0f};

    syNumpy::saveNpy("floats.npy", values);

    syNumpy::NpyArray loaded = syNumpy::loadNpy("floats.npy");
    std::vector<float> roundtrip = loaded.asVector<float>();

    std::ifstream input("floats.npy", std::ios::binary);
    std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };

    syNumpy::NpyArray from_buffer = syNumpy::loadNpyBuffer(bytes.data(), bytes.size());
    std::vector<float> in_memory = from_buffer.asVector<float>();

    return 0;
}
```

See [example.cpp](example.cpp) for a fuller sample covering:

- file save/load
- memory-buffer loading
- append mode
- shaped matrix output

## C++ API Interface

The C++ interface is intentionally small and source-oriented. The public declarations live in `synumpy.hpp`, and that header should be treated as the immediate source of truth during integration.

This README does not attempt to fully document every API detail. For project updates, integration notes, and the official documentation path, use:

- [https://pixlab.io/numpy-cpp-library](https://pixlab.io/numpy-cpp-library)

## Related Products

- [FACEIO](https://faceio.net/) for passwordless facial authentication
- [DocScan Mobile App](https://pixlab.io/mobile-apps/docscan-app) for document, receipt, sign, and ID scanning workflows
- [PixLab API Endpoints](https://pixlab.io/api-endpoints) for the broader developer platform
- [DOCSCAN ID Scan API](https://pixlab.io/id-scan-api/doscan) for scanning passports, driver licenses, national IDs, visas, and other identity documents from around the world

## License

BSD 3-Clause. See the [LICENSE](LICENSE) file.
