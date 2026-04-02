/* Symisc syNumpy: C++ usage sample program.
 * Copyright (C) 2026, Symisc Systems https://pixlab.io/numpy-cpp-library
 * Version 1.9.7
 * For information on licensing, redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
 *       legal@symisc.net
 *       licensing@symisc.net
 *       contact@symisc.net
 * or visit:
 *      https://pixlab.io/numpy-cpp-library
 */
/*
 * Copyright (C) 2026 Symisc Systems, S.U.A.R.L [M.I.A.G Mrad Chams Eddine <chm@symisc.net>].
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL SYMISC SYSTEMS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* $SymiscID: example.cpp v1.9.7 Win11 2026-04-01 stable <contact@symisc.net> $ */

#include "synumpy.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

void printShape(const syNumpy::NpyArray& array) {
    std::cout << "shape=(";
    for (std::size_t i = 0; i < array.shape().size(); ++i) {
        if (i != 0) {
            std::cout << ", ";
        }
        std::cout << array.shape()[i];
    }
    std::cout << ")\n";
}  // namespace

template <typename T>
void printValues(const std::vector<T>& values, const char* label) {
    std::cout << label << ": ";
    for (const T& value : values) {
        std::cout << value << ' ';
    }
    std::cout << "\n";
}

}

int main() {
    try {
        const char* float_path = "floats.npy";
        const char* matrix_path = "matrix.npy";

        // 1) Save a one-dimensional float array.
        const std::vector<float> floats = {1.0f, 2.0f, 3.0f};
        syNumpy::saveNpy(float_path, floats);

        // 2) Load the array back from disk and access it as typed values.
        const syNumpy::NpyArray loaded_floats = syNumpy::loadNpy(float_path);
        std::cout << "Loaded " << float_path << "\n";
        std::cout << "dtype=" << loaded_floats.dtypeDescr() << "\n";
        printShape(loaded_floats);
        printValues(loaded_floats.asVector<float>(), "values");

        // 3) Load the same file from a raw memory buffer.
        std::ifstream input(float_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("failed to open floats.npy for buffer example");
        }
        const std::vector<std::uint8_t> file_bytes{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        };
        const syNumpy::NpyArray buffer_loaded = syNumpy::loadNpyBuffer(file_bytes.data(), file_bytes.size());
        printValues(buffer_loaded.asVector<float>(), "buffer values");

        // 4) Append more data along axis 0.
        const std::vector<float> more_floats = {4.0f, 5.0f};
        syNumpy::saveNpy(float_path, more_floats, "a");
        const syNumpy::NpyArray appended = syNumpy::loadNpy(float_path);
        printValues(appended.asVector<float>(), "appended values");

        // 5) Save and load a 2D matrix using an explicit shape.
        const std::vector<std::int32_t> matrix = {
            1, 2, 3,
            4, 5, 6
        };
        syNumpy::saveNpy(matrix_path, matrix, std::vector<std::size_t>{2, 3});
        const syNumpy::NpyArray loaded_matrix = syNumpy::loadNpy(matrix_path);
        std::cout << "Loaded " << matrix_path << "\n";
        std::cout << "dtype=" << loaded_matrix.dtypeDescr() << "\n";
        printShape(loaded_matrix);
        printValues(loaded_matrix.asVector<std::int32_t>(), "matrix values");

        return 0;
    } catch (const syNumpy::Error& error) {
        std::cerr << "syNumpy error: " << error.what() << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Unexpected error: " << error.what() << "\n";
        return 2;
    }
}
