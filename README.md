# syNumpy

### A **C++17** library for **reading and writing** [NumPy](https://numpy.org/) `.npy` files, with **optional** `.npz` (zip) archive support
#### Documentation & Homepage: [https://pixlab.io/numpy-cpp-library](https://pixlab.io/numpy-cpp-library).

## Features

- **NpyArray**: A container that stores the loaded array’s shape, word size, fortran order, and data in memory.  
- **Load & Save**:  
  - `syNumpy::loadNpy(...)` reads from a file.  
  - `syNumpy::loadNpyBuffer(...)` reads from a raw memory buffer.  
  - `syNumpy::saveNpy(...)` writes or appends data.  
- **Append Mode**: Save with `mode = "a"` to extend the first dimension of an existing file (if shapes match).  
- **Endian Checks**: Warn if the file is big-endian while the system is little-endian (or vice versa). No auto-swapping.  
- **Optional NPZ**: `-DSYNUMPY_ENABLE_NPZ=1` enables zip-based `.npz` archiving (requires **zlib**). Minimal example included.

## Quick Start

1. **Include** `syNumpy.hpp` in your C++17 project.
2. **Compile** with `-std=c++17`.
3. **(Optional)** Define `SYNUMPY_ENABLE_NPZ=1` and link **zlib** (`-lz`) for `.npz` archiving.

### Minimal Example

```cpp
#include "syNumpy.hpp"
#include <iostream>

int main() {
    // 1) Save a float array
    {
        std::vector<float> data = {1.0f, 2.0f, 3.0f};
        syNumpy::saveNpy("floats.npy", data); // overwrites if file exists
    }

    // 2) Load it back
    {
        syNumpy::NpyArray arr = syNumpy::loadNpy("floats.npy");
        std::vector<float> loaded = arr.asVector<float>();
        std::cout << "Loaded " << loaded.size() << " floats: ";
        for (auto f : loaded) std::cout << f << " ";
        std::cout << "\n";
    }

#if SYNUMPY_ENABLE_NPZ
    // 3) Create a .npz archive with multiple arrays
    {
        syNumpy::NpzArchive zip;
        std::vector<int> arr1 = {10, 20, 30};
        std::vector<double> arr2 = {3.14, 2.71};
        zip.addArray("ints", arr1);
        zip.addArray("doubles", arr2);
        zip.save("arrays.npz");
    }
#endif
    return 0;
}
