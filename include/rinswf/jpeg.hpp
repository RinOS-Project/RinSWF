// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include "rinswf/bitmaps.hpp"
#include <cstdint>
#include <vector>
namespace rinswf {
struct JpegAsset {
    std::uint16_t character_id=0;
    std::vector<std::uint8_t> image_data; // JPEG or PNG/GIF in DefineBitsJPEG2/3/4
    std::vector<std::uint8_t> compressed_alpha; // JPEG3/JPEG4 zlib stream
    std::int16_t deblock_fixed8=0; // JPEG4 only
    bool has_alpha=false;
};
struct JpegResult {
    Error error=Error::none;
    std::string message;
    JpegAsset asset;
    explicit operator bool() const noexcept { return error==Error::none; }
};
// Parses JPEG2/3/4 payloads.
[[nodiscard]] JpegResult parse_jpeg_asset(const Tag& tag);
// Uses optional libjpeg when available, otherwise returns unsupported_compression.
[[nodiscard]] BitmapResult decode_jpeg_bitmap(const JpegAsset& asset,
                                              std::size_t max_pixels=16*1024*1024);
[[nodiscard]] Error collect_jpeg_bitmaps(const Document& document,
                                         BitmapLibrary& library,
                                         std::size_t max_pixels=16*1024*1024);
[[nodiscard]] Error decode_jpeg_alpha(const JpegAsset& asset,
                                     std::size_t width, std::size_t height,
                                     std::vector<std::uint8_t>& alpha,
                                     std::size_t max_pixels=16*1024*1024);
}
