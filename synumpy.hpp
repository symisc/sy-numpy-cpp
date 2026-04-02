/* Symisc syNumpy: A C++17 NumPy .npy Reader and Writer Standalone Library.
 * Copyright (C) 2026, Symisc Systems https://pixlab.io/numpy-cpp-library
 * Version 1.9.7
 * For information on licensing, redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES
 * please contact Symisc Systems via:
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
/* $SymiscID: synumpy.hpp v1.9.7 Win11 2026-04-01 stable <contact@symisc.net> $ */
#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace syNumpy {

inline constexpr int kVersionMajor = 1;
inline constexpr int kVersionMinor = 9;
inline constexpr int kVersionPatch = 7;
inline constexpr std::string_view kVersionString = "1.9.7";

/// Exception type used for parse, validation, and I/O failures.
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message);
};

/// Owns one `.npy` array in memory together with its NumPy metadata.
///
/// The array stores:
/// - the normalized NumPy dtype descriptor, for example `"<f4"` or `"|u1"`
/// - the original shape
/// - the declared Fortran-order flag
/// - a contiguous byte buffer containing the payload
///
/// `NpyArray` does not perform implicit dtype conversion. Typed accessors require
/// an exact descriptor match and throw `syNumpy::Error` on mismatch.
class NpyArray {
public:
    NpyArray() = default;
    NpyArray(std::vector<std::size_t> shape, std::string dtype_descr, bool fortran_order);

    /// Returns the NumPy shape tuple.
    const std::vector<std::size_t>& shape() const noexcept;
    /// Returns the normalized NumPy dtype descriptor.
    std::string_view dtypeDescr() const noexcept;
    /// Returns the dtype item size in bytes.
    std::size_t wordSize() const noexcept;
    /// Returns the `fortran_order` flag from the `.npy` header.
    bool fortranOrder() const noexcept;
    /// Returns the total number of logical values in the array.
    std::size_t numValues() const noexcept;
    /// Returns the payload size in bytes.
    std::size_t numBytes() const noexcept;

    /// Returns the raw payload as a byte pointer.
    const std::uint8_t* bytes() const noexcept;
    /// Returns the mutable raw payload as a byte pointer.
    std::uint8_t* bytes() noexcept;

    /// Returns `true` when the array dtype exactly matches `T`.
    template <typename T>
    bool is() const noexcept;

    /// Returns a typed read-only pointer to the payload.
    ///
    /// Throws `syNumpy::Error` if the stored dtype descriptor does not match `T`.
    template <typename T>
    const T* data() const;

    /// Returns a typed mutable pointer to the payload.
    ///
    /// Throws `syNumpy::Error` if the stored dtype descriptor does not match `T`.
    template <typename T>
    T* data();

    /// Copies the payload into a `std::vector<T>`.
    ///
    /// Throws `syNumpy::Error` if the stored dtype descriptor does not match `T`.
    template <typename T>
    std::vector<T> asVector() const;

private:
    std::vector<std::size_t> shape_;
    std::string dtype_descr_;
    std::size_t word_size_ = 0;
    bool fortran_order_ = false;
    std::size_t num_values_ = 0;
    std::size_t num_bytes_ = 0;
    std::shared_ptr<std::uint8_t> data_;
};

/// Parses a complete `.npy` image already loaded in memory.
///
/// `buffer` must point to the first byte of the file and `size` must cover the
/// full header and payload. This is the core parser entry point used by the
/// implementation.
///
/// Throws `syNumpy::Error` on malformed input, unsupported dtypes, unsupported
/// endianness, or truncated payloads.
NpyArray loadNpyBuffer(const void* buffer, std::size_t size);

/// Loads a `.npy` file from disk and returns the fully materialized array.
NpyArray loadNpy(const std::filesystem::path& path);

/// Low-level save API for callers that already have a NumPy dtype descriptor.
///
/// `byte_count` must exactly match `shape * dtype item size`.
///
/// Supported modes:
/// - `"w"` / `"wb"`: overwrite or create the file
/// - `"a"` / `"ab"`: append along axis 0 if rank, dtype, and trailing dimensions match
///
/// Append mode may rewrite the whole file when the expanded header no longer
/// fits in place.
void saveNpyRaw(
    const std::filesystem::path& path,
    const void* data,
    std::size_t byte_count,
    const std::vector<std::size_t>& shape,
    std::string_view dtype_descr,
    std::string_view mode = "w");

/// Internal helpers used by the header-only templates below.
/// Not part of the stable external API.
namespace detail {

template <typename T>
struct AlwaysFalse : std::false_type {};

inline char hostEndianChar() noexcept {
    const std::uint16_t value = 0x0102;
    return (*reinterpret_cast<const std::uint8_t*>(&value) == 0x02) ? '<' : '>';
}

inline std::size_t shapeElementCount(const std::vector<std::size_t>& shape) {
    std::size_t count = 1u;
    for (std::size_t dim : shape) {
        if (dim == 0) {
            return 0u;
        }
        if (count > (static_cast<std::size_t>(-1) / dim)) {
            throw Error("saveNpy overflow while computing element count");
        }
        count *= dim;
    }
    return count;
}

template <typename T>
inline std::string dtypeDescriptor() {
    using U = std::remove_cv_t<T>;
    const char endian = hostEndianChar();

    if constexpr (std::is_same_v<U, bool>) {
        return "|b1";
    } else if constexpr (std::is_same_v<U, std::int8_t>) {
        return "|i1";
    } else if constexpr (std::is_same_v<U, std::uint8_t>) {
        return "|u1";
    } else if constexpr (std::is_same_v<U, char>) {
        return std::is_signed_v<char> ? "|i1" : "|u1";
    } else if constexpr (std::is_same_v<U, signed char>) {
        return "|i1";
    } else if constexpr (std::is_same_v<U, unsigned char>) {
        return "|u1";
    } else if constexpr (std::is_same_v<U, std::int16_t>) {
        return std::string(1, endian) + "i2";
    } else if constexpr (std::is_same_v<U, std::uint16_t>) {
        return std::string(1, endian) + "u2";
    } else if constexpr (std::is_same_v<U, std::int32_t>) {
        return std::string(1, endian) + "i4";
    } else if constexpr (std::is_same_v<U, std::uint32_t>) {
        return std::string(1, endian) + "u4";
    } else if constexpr (std::is_same_v<U, std::int64_t>) {
        return std::string(1, endian) + "i8";
    } else if constexpr (std::is_same_v<U, std::uint64_t>) {
        return std::string(1, endian) + "u8";
    } else if constexpr (std::is_same_v<U, float>) {
        return std::string(1, endian) + "f4";
    } else if constexpr (std::is_same_v<U, double>) {
        return std::string(1, endian) + "f8";
    } else if constexpr (std::is_same_v<U, long double>) {
        return std::string(1, endian) + "f" + std::to_string(sizeof(long double));
    } else if constexpr (std::is_same_v<U, std::complex<float>>) {
        return std::string(1, endian) + "c8";
    } else if constexpr (std::is_same_v<U, std::complex<double>>) {
        return std::string(1, endian) + "c16";
    } else if constexpr (std::is_same_v<U, std::complex<long double>>) {
        return std::string(1, endian) + "c" + std::to_string(sizeof(std::complex<long double>));
    } else {
        static_assert(AlwaysFalse<U>::value, "Unsupported NumPy element type");
    }
}

}  // namespace detail

template <typename T>
inline bool NpyArray::is() const noexcept {
    return dtype_descr_ == detail::dtypeDescriptor<T>();
}

template <typename T>
inline const T* NpyArray::data() const {
    if (!is<T>()) {
        throw Error("NpyArray::data<T> type mismatch: array dtype is '" + dtype_descr_ +
                    "', requested '" + detail::dtypeDescriptor<T>() + "'");
    }
    return reinterpret_cast<const T*>(data_.get());
}

template <typename T>
inline T* NpyArray::data() {
    if (!is<T>()) {
        throw Error("NpyArray::data<T> type mismatch: array dtype is '" + dtype_descr_ +
                    "', requested '" + detail::dtypeDescriptor<T>() + "'");
    }
    return reinterpret_cast<T*>(data_.get());
}

template <typename T>
inline std::vector<T> NpyArray::asVector() const {
    if (!is<T>()) {
        throw Error("NpyArray::asVector<T> type mismatch: array dtype is '" + dtype_descr_ +
                    "', requested '" + detail::dtypeDescriptor<T>() + "'");
    }

    std::vector<T> out(num_values_);
    if (num_bytes_ != 0) {
        std::memcpy(out.data(), data_.get(), num_bytes_);
    }
    return out;
}

template <typename T>
inline void saveNpy(
    const std::filesystem::path& path,
    const T* data,
    const std::vector<std::size_t>& shape,
    std::string_view mode = "w") {
    const std::size_t count = detail::shapeElementCount(shape);
    if (count > (static_cast<std::size_t>(-1) / sizeof(T))) {
        throw Error("saveNpy overflow while computing byte count");
    }

    const std::size_t byte_count = count * sizeof(T);
    if (byte_count != 0 && data == nullptr) {
        throw Error("saveNpy received a null data pointer for non-empty input");
    }

    saveNpyRaw(path, data, byte_count, shape, detail::dtypeDescriptor<T>(), mode);
}

/// Saves a typed array using an explicit NumPy shape.
///
/// `shape = {}` represents a scalar and therefore expects exactly one element.
/// The product of `shape` must match `data.size()`, otherwise the save fails.
template <typename T>
inline void saveNpy(
    const std::filesystem::path& path,
    const std::vector<T>& data,
    const std::vector<std::size_t>& shape,
    std::string_view mode = "w") {
    const std::size_t expected_count = detail::shapeElementCount(shape);
    if (data.size() != expected_count) {
        throw Error("saveNpy explicit shape does not match the vector length");
    }
    saveNpy(path, data.empty() ? nullptr : data.data(), shape, mode);
}

/// Saves a typed one-dimensional vector.
///
/// This overload uses `shape = {data.size()}`.
template <typename T>
inline void saveNpy(
    const std::filesystem::path& path,
    const std::vector<T>& data,
    std::string_view mode = "w") {
    saveNpy(path, data, std::vector<std::size_t>{data.size()}, mode);
}

}  // namespace syNumpy
