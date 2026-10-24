// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <map>
#include <vector>
namespace rinswf {
struct Bitmap {
    std::uint16_t character_id=0, width=0, height=0;
    std::vector<std::uint8_t> rgba; // straight-alpha RGBA
};
struct BitmapResult {
    Error error=Error::none;
    std::string message;
    Bitmap bitmap;
    explicit operator bool() const noexcept { return error==Error::none; }
};
// Decode DefineBitsLossless (20) and DefineBitsLossless2 (36).
// Other bitmap formats, including JPEG, return an explicit error.
[[nodiscard]] BitmapResult decode_lossless(const Tag& tag, std::size_t max_pixels=16*1024*1024);
struct BitmapLibrary {
    std::map<std::uint16_t,Bitmap> images;
    [[nodiscard]] const Bitmap* find(std::uint16_t id) const noexcept {
        const auto it=images.find(id);
        return it==images.end()?nullptr:&it->second;
    }
};
[[nodiscard]] Error collect_lossless(const Document& document, BitmapLibrary& library,
                                    std::size_t max_pixels=16*1024*1024);
}
