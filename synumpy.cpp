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
/* $SymiscID: synumpy.cpp v1.9.7 Win11 2026-04-01 stable <contact@symisc.net> $ */
#include "synumpy.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace syNumpy {

namespace {

constexpr int kImplementationVersionMajor = 1;
constexpr int kImplementationVersionMinor = 9;
constexpr int kImplementationVersionPatch = 7;

static_assert(
    syNumpy::kVersionMajor == kImplementationVersionMajor &&
    syNumpy::kVersionMinor == kImplementationVersionMinor &&
    syNumpy::kVersionPatch == kImplementationVersionPatch,
    "synumpy version mismatch between header and source");

constexpr std::array<unsigned char, 6> kMagic{{0x93, 'N', 'U', 'M', 'P', 'Y'}};
constexpr std::size_t kAlignment = 64;

struct ParsedHeader {
    std::string dtype_descr;
    bool fortran_order = false;
    std::vector<std::size_t> shape;
    std::size_t word_size = 0;
    std::size_t num_values = 0;
    std::size_t num_bytes = 0;
    std::size_t data_offset = 0;
    std::uint8_t major_version = 0;
    std::uint8_t minor_version = 0;
};

[[noreturn]] void fail(const std::string& message) {
    throw Error(message);
}

std::string trim(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string(value.substr(start, end - start));
}

std::size_t checkedMultiply(std::size_t lhs, std::size_t rhs, const char* what) {
    if (lhs == 0 || rhs == 0) {
        return 0;
    }
    if (lhs > (std::numeric_limits<std::size_t>::max() / rhs)) {
        fail(std::string("overflow while computing ") + what);
    }
    return lhs * rhs;
}

std::size_t checkedAdd(std::size_t lhs, std::size_t rhs, const char* what) {
    if (lhs > (std::numeric_limits<std::size_t>::max() - rhs)) {
        fail(std::string("overflow while computing ") + what);
    }
    return lhs + rhs;
}

std::size_t elementCount(const std::vector<std::size_t>& shape) {
    std::size_t count = 1;
    for (std::size_t dim : shape) {
        count = checkedMultiply(count, dim, "element count");
    }
    return count;
}

std::size_t parseUnsigned(std::string_view text, const char* field_name) {
    const std::string token = trim(text);
    if (token.empty()) {
        fail(std::string("missing unsigned integer for ") + field_name);
    }

    std::size_t value = 0;
    for (char ch : token) {
        if (ch < '0' || ch > '9') {
            fail(std::string("invalid unsigned integer for ") + field_name + ": '" + token + "'");
        }
        const std::size_t digit = static_cast<std::size_t>(ch - '0');
        if (value > ((std::numeric_limits<std::size_t>::max() - digit) / 10u)) {
            fail(std::string("overflow while parsing ") + field_name);
        }
        value = (value * 10u) + digit;
    }
    return value;
}

std::size_t parseItemSize(std::string_view dtype_descr) {
    if (dtype_descr.size() < 3) {
        fail("dtype descriptor is too short: '" + std::string(dtype_descr) + "'");
    }
    return parseUnsigned(dtype_descr.substr(2), "dtype item size");
}

std::string normalizeDescr(std::string_view dtype_descr) {
    if (dtype_descr.size() < 3) {
        fail("unsupported dtype descriptor: '" + std::string(dtype_descr) + "'");
    }

    char endian = dtype_descr[0];
    const char kind = dtype_descr[1];
    const std::size_t item_size = parseItemSize(dtype_descr);

    switch (kind) {
    case 'b':
    case 'i':
    case 'u':
    case 'f':
    case 'c':
        break;
    default:
        fail("unsupported dtype kind: '" + std::string(1, kind) + "'");
    }

    if (kind == 'b') {
        if (item_size != 1) {
            fail("unsupported bool dtype size: '" + std::string(dtype_descr) + "'");
        }
        return "|b1";
    }

    if (item_size == 1) {
        if (kind == 'f' || kind == 'c') {
            fail("unsupported single-byte floating or complex dtype: '" + std::string(dtype_descr) + "'");
        }
        return std::string("|") + kind + "1";
    }

    if (endian == '=') {
        // NumPy uses '=' for native-endian payloads. Normalize it to an
        // explicit host-endian descriptor so comparisons stay deterministic.
        endian = detail::hostEndianChar();
    }

    if (endian != '<' && endian != '>') {
        fail("unsupported dtype endianness: '" + std::string(1, endian) + "'");
    }

    if (endian != detail::hostEndianChar()) {
        fail("dtype endianness does not match host endianness: '" + std::string(dtype_descr) + "'");
    }

    return std::string(1, endian) + kind + std::to_string(item_size);
}

std::size_t headerLengthFieldSize(std::uint8_t major_version) {
    switch (major_version) {
    case 1:
        return 2;
    case 2:
    case 3:
        return 4;
    default:
        fail("unsupported .npy format version: " + std::to_string(major_version));
    }
}

std::size_t readLittleEndianUnsigned(const std::uint8_t* bytes, std::size_t width) {
    std::size_t value = 0;
    for (std::size_t i = 0; i < width; ++i) {
        value |= (static_cast<std::size_t>(bytes[i]) << (8u * i));
    }
    return value;
}

std::string parseQuotedField(std::string_view header, std::string_view key) {
    const std::string quoted_key = "'" + std::string(key) + "'";
    const std::size_t key_pos = header.find(quoted_key);
    if (key_pos == std::string_view::npos) {
        fail("missing header field '" + std::string(key) + "'");
    }

    const std::size_t colon_pos = header.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string_view::npos) {
        fail("invalid header field '" + std::string(key) + "'");
    }

    std::size_t value_pos = colon_pos + 1;
    while (value_pos < header.size() &&
           std::isspace(static_cast<unsigned char>(header[value_pos])) != 0) {
        ++value_pos;
    }
    if (value_pos >= header.size()) {
        fail("missing value for header field '" + std::string(key) + "'");
    }

    const char quote = header[value_pos];
    if (quote != '\'' && quote != '"') {
        fail("header field '" + std::string(key) + "' is not a plain string");
    }

    const std::size_t value_end = header.find(quote, value_pos + 1);
    if (value_end == std::string_view::npos) {
        fail("unterminated string value for header field '" + std::string(key) + "'");
    }

    return std::string(header.substr(value_pos + 1, value_end - value_pos - 1));
}

bool parseBoolField(std::string_view header, std::string_view key) {
    const std::string quoted_key = "'" + std::string(key) + "'";
    const std::size_t key_pos = header.find(quoted_key);
    if (key_pos == std::string_view::npos) {
        fail("missing header field '" + std::string(key) + "'");
    }

    const std::size_t colon_pos = header.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string_view::npos) {
        fail("invalid header field '" + std::string(key) + "'");
    }

    std::size_t value_pos = colon_pos + 1;
    while (value_pos < header.size() &&
           std::isspace(static_cast<unsigned char>(header[value_pos])) != 0) {
        ++value_pos;
    }

    if (header.compare(value_pos, 4, "True") == 0) {
        return true;
    }
    if (header.compare(value_pos, 5, "False") == 0) {
        return false;
    }

    fail("invalid boolean value for header field '" + std::string(key) + "'");
}

std::vector<std::size_t> parseShapeField(std::string_view header) {
    const std::string quoted_key = "'shape'";
    const std::size_t key_pos = header.find(quoted_key);
    if (key_pos == std::string_view::npos) {
        fail("missing header field 'shape'");
    }

    const std::size_t colon_pos = header.find(':', key_pos + quoted_key.size());
    if (colon_pos == std::string_view::npos) {
        fail("invalid header field 'shape'");
    }

    std::size_t tuple_start = colon_pos + 1;
    while (tuple_start < header.size() &&
           std::isspace(static_cast<unsigned char>(header[tuple_start])) != 0) {
        ++tuple_start;
    }

    if (tuple_start >= header.size() || header[tuple_start] != '(') {
        fail("shape field is not a tuple");
    }

    const std::size_t tuple_end = header.find(')', tuple_start + 1);
    if (tuple_end == std::string_view::npos) {
        fail("unterminated shape tuple");
    }

    const std::string inner = trim(header.substr(tuple_start + 1, tuple_end - tuple_start - 1));
    if (inner.empty()) {
        return {};
    }

    std::vector<std::size_t> shape;
    std::size_t token_start = 0;
    while (token_start < inner.size()) {
        const std::size_t comma = inner.find(',', token_start);
        const std::size_t token_end = (comma == std::string::npos) ? inner.size() : comma;
        const std::string token = trim(std::string_view(inner).substr(token_start, token_end - token_start));
        if (!token.empty()) {
            shape.push_back(parseUnsigned(token, "shape"));
        }
        if (comma == std::string::npos) {
            break;
        }
        token_start = comma + 1;
    }

    return shape;
}

ParsedHeader parseHeaderPrefix(const std::uint8_t* bytes, std::size_t size) {
    if (size < 10) {
        fail("buffer is too small to contain a valid .npy header");
    }

    if (!std::equal(kMagic.begin(), kMagic.end(), bytes)) {
        fail("buffer does not start with a valid .npy magic header");
    }

    ParsedHeader result;
    result.major_version = bytes[6];
    result.minor_version = bytes[7];

    const std::size_t length_field_size = headerLengthFieldSize(result.major_version);
    const std::size_t prefix_size = 8 + length_field_size;
    if (size < prefix_size) {
        fail("buffer ended before the full .npy prefix could be read");
    }

    const std::size_t header_length = readLittleEndianUnsigned(bytes + 8, length_field_size);
    result.data_offset = prefix_size + header_length;
    if (size < result.data_offset) {
        fail("buffer ended before the declared .npy header finished");
    }

    const std::string_view header(
        reinterpret_cast<const char*>(bytes + prefix_size),
        header_length);

    // Only plain scalar descriptors are accepted here. Structured, object,
    // and unicode dtypes intentionally stay out of scope for this library.
    result.dtype_descr = normalizeDescr(parseQuotedField(header, "descr"));
    result.fortran_order = parseBoolField(header, "fortran_order");
    result.shape = parseShapeField(header);
    result.word_size = parseItemSize(result.dtype_descr);
    result.num_values = elementCount(result.shape);
    result.num_bytes = checkedMultiply(result.num_values, result.word_size, "array byte size");
    return result;
}

std::string shapeToString(const std::vector<std::size_t>& shape) {
    std::ostringstream stream;
    stream << "(";
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i != 0) {
            stream << ", ";
        }
        stream << shape[i];
    }
    if (shape.size() == 1) {
        stream << ",";
    }
    stream << ")";
    return stream.str();
}

std::vector<std::uint8_t> buildHeader(std::string_view dtype_descr, const std::vector<std::size_t>& shape) {
    const std::string dict =
        "{'descr': '" + std::string(dtype_descr) + "', 'fortran_order': False, 'shape': " +
        shapeToString(shape) + ", }";

    std::uint8_t major_version = 1;
    std::uint8_t minor_version = 0;
    std::size_t length_field_size = 2;
    std::string header_text;

    for (;;) {
        const std::size_t prefix_size = 8 + length_field_size;
        // NumPy expects the full preamble+header block to align cleanly.
        // Pad with spaces and end with '\n' to produce a valid header record.
        const std::size_t total_without_padding = prefix_size + dict.size() + 1;
        const std::size_t padding = (kAlignment - (total_without_padding % kAlignment)) % kAlignment;
        header_text = dict;
        header_text.append(padding, ' ');
        header_text.push_back('\n');

        if (major_version == 1 && header_text.size() > std::numeric_limits<std::uint16_t>::max()) {
            // Version 1 stores the header length in 16 bits. Promote to
            // version 2 when the serialized dictionary no longer fits.
            major_version = 2;
            length_field_size = 4;
            continue;
        }
        break;
    }

    std::vector<std::uint8_t> out;
    out.reserve(8 + length_field_size + header_text.size());
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    out.push_back(major_version);
    out.push_back(minor_version);

    if (length_field_size == 2) {
        const std::uint16_t header_len = static_cast<std::uint16_t>(header_text.size());
        out.push_back(static_cast<std::uint8_t>(header_len & 0xffu));
        out.push_back(static_cast<std::uint8_t>((header_len >> 8u) & 0xffu));
    } else {
        const std::uint32_t header_len = static_cast<std::uint32_t>(header_text.size());
        out.push_back(static_cast<std::uint8_t>(header_len & 0xffu));
        out.push_back(static_cast<std::uint8_t>((header_len >> 8u) & 0xffu));
        out.push_back(static_cast<std::uint8_t>((header_len >> 16u) & 0xffu));
        out.push_back(static_cast<std::uint8_t>((header_len >> 24u) & 0xffu));
    }

    out.insert(out.end(), header_text.begin(), header_text.end());
    return out;
}

std::string normalizeMode(std::string_view mode) {
    if (mode.empty()) {
        return "w";
    }
    if (mode == "w" || mode == "wb") {
        return "w";
    }
    if (mode == "a" || mode == "ab") {
        return "a";
    }
    fail("unsupported save mode: '" + std::string(mode) + "'");
}

bool fileExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::vector<std::uint8_t> readExact(std::istream& stream, std::size_t count, const char* context) {
    std::vector<std::uint8_t> buffer(count);
    if (count == 0) {
        return buffer;
    }

    stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(count));
    if (stream.gcount() != static_cast<std::streamsize>(count)) {
        fail(std::string("failed to read ") + context);
    }
    return buffer;
}

ParsedHeader readHeaderFromFile(std::ifstream& stream) {
    // Read the fixed 8-byte prologue first, then read the version-dependent
    // header-length field and payload header bytes.
    const std::vector<std::uint8_t> prologue = readExact(stream, 8, "file header prologue");
    if (!std::equal(kMagic.begin(), kMagic.end(), prologue.begin())) {
        fail("file does not start with a valid .npy magic header");
    }

    const std::uint8_t major_version = prologue[6];
    const std::size_t length_field_size = headerLengthFieldSize(major_version);
    const std::vector<std::uint8_t> length_bytes = readExact(stream, length_field_size, "header length");
    const std::size_t prefix_size = 8 + length_field_size;
    const std::size_t header_length = readLittleEndianUnsigned(length_bytes.data(), length_field_size);

    std::vector<std::uint8_t> header_bytes(prefix_size + header_length);
    std::copy(prologue.begin(), prologue.end(), header_bytes.begin());
    std::copy(length_bytes.begin(), length_bytes.end(), header_bytes.begin() + 8);

    if (header_length != 0) {
        stream.read(
            reinterpret_cast<char*>(header_bytes.data() + prefix_size),
            static_cast<std::streamsize>(header_length));
        if (stream.gcount() != static_cast<std::streamsize>(header_length)) {
            fail("failed to read full .npy header");
        }
    }

    return parseHeaderPrefix(header_bytes.data(), header_bytes.size());
}

void writeBytes(std::ostream& stream, const void* data, std::size_t size, const char* context) {
    if (size == 0) {
        return;
    }
    stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    if (!stream) {
        fail(std::string("failed to write ") + context);
    }
}

void copyBytes(std::istream& input, std::ostream& output, std::size_t count) {
    std::array<char, 1 << 20> buffer{};
    std::size_t remaining = count;

    while (remaining != 0) {
        const std::size_t chunk = std::min<std::size_t>(remaining, buffer.size());
        input.read(buffer.data(), static_cast<std::streamsize>(chunk));
        if (input.gcount() != static_cast<std::streamsize>(chunk)) {
            fail("failed while copying existing array data during append");
        }
        output.write(buffer.data(), static_cast<std::streamsize>(chunk));
        if (!output) {
            fail("failed while writing appended array data");
        }
        remaining -= chunk;
    }
}

std::filesystem::path makeTempPath(const std::filesystem::path& path) {
    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::filesystem::path temp = path;
    temp += ".";
    temp += std::to_string(ticks);
    temp += ".tmp";
    return temp;
}

void replaceFile(const std::filesystem::path& source, const std::filesystem::path& target) {
#ifdef _WIN32
    const std::wstring from = source.wstring();
    const std::wstring to = target.wstring();
    if (::MoveFileExW(
            from.c_str(),
            to.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        const DWORD code = ::GetLastError();
        fail("failed to replace file '" + target.string() + "' (Win32 error " +
             std::to_string(static_cast<unsigned long>(code)) + ")");
    }
#else
    std::error_code ec;
    std::filesystem::rename(source, target, ec);
    if (ec) {
        fail("failed to replace file '" + target.string() + "': " + ec.message());
    }
#endif
}

void writeFreshFile(
    const std::filesystem::path& path,
    const std::vector<std::uint8_t>& header,
    const void* data,
    std::size_t byte_count) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        fail("unable to open '" + path.string() + "' for writing");
    }

    writeBytes(output, header.data(), header.size(), "NPY header");
    writeBytes(output, data, byte_count, "NPY payload");
}

void rewriteForAppend(
    const std::filesystem::path& path,
    const ParsedHeader& existing,
    const std::vector<std::uint8_t>& new_header,
    const void* append_data,
    std::size_t append_bytes) {
    // If the updated shape makes the serialized header longer, the payload
    // offset changes and the file must be rewritten instead of patched in place.
    const std::filesystem::path temp_path = makeTempPath(path);

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("unable to reopen '" + path.string() + "' for append rewrite");
    }

    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        fail("unable to create temporary file '" + temp_path.string() + "'");
    }

    writeBytes(output, new_header.data(), new_header.size(), "replacement NPY header");

    input.seekg(static_cast<std::streamoff>(existing.data_offset), std::ios::beg);
    if (!input) {
        fail("failed to seek existing payload in '" + path.string() + "'");
    }

    copyBytes(input, output, existing.num_bytes);
    writeBytes(output, append_data, append_bytes, "appended NPY payload");

    output.close();
    input.close();
    replaceFile(temp_path, path);
}

void validateSaveArguments(
    const void* data,
    std::size_t byte_count,
    const std::vector<std::size_t>& shape,
    std::string_view dtype_descr) {
    const std::string normalized_descr = normalizeDescr(dtype_descr);
    const std::size_t item_size = parseItemSize(normalized_descr);
    const std::size_t values = elementCount(shape);
    const std::size_t expected_bytes = checkedMultiply(values, item_size, "save byte count");

    if (expected_bytes != byte_count) {
        fail("saveNpyRaw byte count does not match shape and dtype");
    }

    if (byte_count != 0 && data == nullptr) {
        fail("saveNpyRaw received a null data pointer for non-empty input");
    }
}

}  // namespace

Error::Error(const std::string& message)
    : std::runtime_error(message) {}

NpyArray::NpyArray(std::vector<std::size_t> shape, std::string dtype_descr, bool fortran_order)
    : shape_(std::move(shape)),
      dtype_descr_(normalizeDescr(dtype_descr)),
      word_size_(parseItemSize(dtype_descr_)),
      fortran_order_(fortran_order),
      num_values_(elementCount(shape_)),
      num_bytes_(checkedMultiply(num_values_, word_size_, "array byte size")) {
    if (num_bytes_ != 0) {
        data_ = std::shared_ptr<std::uint8_t>(new std::uint8_t[num_bytes_], std::default_delete<std::uint8_t[]>());
    }
}

const std::vector<std::size_t>& NpyArray::shape() const noexcept {
    return shape_;
}

std::string_view NpyArray::dtypeDescr() const noexcept {
    return dtype_descr_;
}

std::size_t NpyArray::wordSize() const noexcept {
    return word_size_;
}

bool NpyArray::fortranOrder() const noexcept {
    return fortran_order_;
}

std::size_t NpyArray::numValues() const noexcept {
    return num_values_;
}

std::size_t NpyArray::numBytes() const noexcept {
    return num_bytes_;
}

const std::uint8_t* NpyArray::bytes() const noexcept {
    return data_.get();
}

std::uint8_t* NpyArray::bytes() noexcept {
    return data_.get();
}

NpyArray loadNpyBuffer(const void* buffer, std::size_t size) {
    if (buffer == nullptr) {
        fail("loadNpyBuffer received a null buffer");
    }

    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    const ParsedHeader parsed = parseHeaderPrefix(bytes, size);

    if ((size - parsed.data_offset) < parsed.num_bytes) {
        fail("buffer ended before the full array payload was available");
    }

    NpyArray array(parsed.shape, parsed.dtype_descr, parsed.fortran_order);
    if (parsed.num_bytes != 0) {
        std::memcpy(array.bytes(), bytes + parsed.data_offset, parsed.num_bytes);
    }
    return array;
}

NpyArray loadNpy(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("unable to open '" + path.string() + "' for reading");
    }

    const ParsedHeader parsed = readHeaderFromFile(input);
    NpyArray array(parsed.shape, parsed.dtype_descr, parsed.fortran_order);

    if (parsed.num_bytes != 0) {
        input.read(reinterpret_cast<char*>(array.bytes()), static_cast<std::streamsize>(parsed.num_bytes));
        if (input.gcount() != static_cast<std::streamsize>(parsed.num_bytes)) {
            fail("failed to read full payload from '" + path.string() + "'");
        }
    }

    return array;
}

void saveNpyRaw(
    const std::filesystem::path& path,
    const void* data,
    std::size_t byte_count,
    const std::vector<std::size_t>& shape,
    std::string_view dtype_descr,
    std::string_view mode) {
    validateSaveArguments(data, byte_count, shape, dtype_descr);

    const std::string normalized_mode = normalizeMode(mode);
    const std::string normalized_descr = normalizeDescr(dtype_descr);

    if (normalized_mode == "w" || !fileExists(path)) {
        const std::vector<std::uint8_t> header = buildHeader(normalized_descr, shape);
        writeFreshFile(path, header, data, byte_count);
        return;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("unable to open '" + path.string() + "' for append");
    }

    const ParsedHeader existing = readHeaderFromFile(input);
    if (existing.fortran_order) {
        fail("append mode does not support Fortran-order arrays");
    }
    if (shape.empty() || existing.shape.empty()) {
        fail("append mode requires arrays with at least one dimension");
    }
    if (existing.dtype_descr != normalized_descr) {
        fail("append mode requires the same dtype as the existing file");
    }
    if (existing.shape.size() != shape.size()) {
        fail("append mode requires the same rank as the existing file");
    }
    for (std::size_t i = 1; i < shape.size(); ++i) {
        if (existing.shape[i] != shape[i]) {
            fail("append mode requires all dimensions except axis 0 to match");
        }
    }

    std::vector<std::size_t> merged_shape = existing.shape;
    merged_shape[0] = checkedAdd(merged_shape[0], shape[0], "append axis size");

    const std::vector<std::uint8_t> new_header = buildHeader(normalized_descr, merged_shape);
    if (new_header.size() == existing.data_offset) {
        std::fstream io(path, std::ios::binary | std::ios::in | std::ios::out);
        if (!io) {
            fail("unable to reopen '" + path.string() + "' for in-place append");
        }
        io.seekp(0, std::ios::beg);
        writeBytes(io, new_header.data(), new_header.size(), "updated NPY header");
        io.seekp(0, std::ios::end);
        writeBytes(io, data, byte_count, "appended NPY payload");
        return;
    }

    rewriteForAppend(path, existing, new_header, data, byte_count);
}

}  // namespace syNumpy
