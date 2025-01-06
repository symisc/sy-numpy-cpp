# syNumpy

### A **C++17** library for **reading and writing** [NumPy](https://numpy.org/) `.npy` files, with **optional** `.npz` (zip) archive support
#### Documentation & Homepage: [https://pixlab.io/numpy-cpp-library](https://pixlab.io/numpy-cpp-library).

---

## Features

- **Float or Double**: By default, syNumpy only supports **`float`** and **`double`** arrays.  
- **Read & Write**: Single- or multi-dimensional `.npy` files (NumPy versions **1.0**, **2.0**, **3.0**).  
- **Endian Check**: Warns if the file’s byte order differs from the host system. (Byte-swapping is **not** yet implemented.)  
- **Optional NPZ**: If `SYNUMPY_ENABLE_NPZ=1` is defined, syNumpy can zip/unzip multiple `.npy` arrays into a single `.npz` file. Otherwise, no external dependencies are required.

---

## Quick Start

1. **Include** [syNumpy.hpp](syNumpy.hpp) in your project.  
2. **Compile** with `-std=c++17`.  
3. (Optional) Define `SYNUMPY_ENABLE_NPZ=1` and link **zlib** (`-lz`) to enable `.npz` support.

### Minimal Example

```cpp
#include "syNumpy.hpp"
#include <iostream>

int main()
{
    // Save a float array
    std::vector<float> data = {1.0f, 2.0f, 3.0f};
    std::vector<size_t> shape = {3};
    syNumpy::SaveNpy("float_test.npy", data, shape, syNumpy::NpyVersion::V2_0);

    // Load the array
    {
        std::vector<float> loaded;
        std::vector<size_t> shapeLoaded;
        syNumpy::LoadNpy("float_test.npy", loaded, shapeLoaded);
        std::cout << "Shape: (";
        for (auto s : shapeLoaded) std::cout << s << " ";
        std::cout << ")\nData: ";
        for (auto val : loaded) std::cout << val << " ";
        std::cout << std::endl;
    }

#if SYNUMPY_ENABLE_NPZ
    // If NPZ is enabled, create an archive with a double array
    {
        syNumpy::NpzArchive archive;
        std::vector<double> arr = {3.14, 2.71, 1.41};
        archive.AddArray("my_double_array", arr, {3}, syNumpy::NpyVersion::V2_0);
        archive.Save("test.npz");
    }

    // Load from the .npz
    {
        syNumpy::NpzArchive archive;
        archive.Load("test.npz");
        std::vector<double> out;
        std::vector<size_t> shapeOut;
        archive.GetArray("my_double_array", out, shapeOut);

        std::cout << "my_double_array shape: (";
        for (auto s : shapeOut) std::cout << s << " ";
        std::cout << ")\nData: ";
        for (auto val : out) std::cout << val << " ";
        std::cout << std::endl;
    }
#endif

    return 0;
}

