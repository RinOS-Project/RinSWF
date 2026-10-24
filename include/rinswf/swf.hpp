// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rinswf {
enum class Error {
    none, truncated, invalid_signature, unsupported_compression,
    invalid_length, size_limit, malformed_tag, decompression_failed
};
struct Header {
    std::uint8_t version = 0;
    std::uint32_t declared_size = 0;
    std::int32_t xmin = 0, xmax = 0, ymin = 0, ymax = 0; // twips
    std::uint16_t frame_rate_fixed8 = 0;
    std::uint16_t frame_count = 0;
};
struct Tag {
    std::uint16_t code = 0;
    std::size_t offset = 0; // offset in uncompressed SWF byte stream
    std::vector<std::uint8_t> data;
};
struct Document {
    Header header;
    std::vector<Tag> tags;
};
struct Result {
    Error error = Error::none;
    std::string message;
    Document document;
    explicit operator bool() const noexcept { return error == Error::none; }
};
struct Limits {
    std::size_t max_uncompressed_bytes = 64 * 1024 * 1024;
    std::size_t max_tags = 100000;
    std::size_t max_tag_bytes = 32 * 1024 * 1024;
};
[[nodiscard]] Result parse(std::span<const std::uint8_t> bytes, Limits limits = {});
[[nodiscard]] const char* error_string(Error error) noexcept;
} // namespace rinswf
