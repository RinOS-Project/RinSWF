// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include "rinswf/shapes.hpp"
#include <cstdint>
#include <vector>
namespace rinswf {
// DefineFont (tag 10) stores individual SHAPE records without fill/line styles.
// Glyph SHAPE records are decoded into vector outlines and their bytes retained.
struct FontGlyph {
    std::vector<std::uint8_t> shape_bytes;
    ShapeGeometry outline;
};
struct FontAsset {
    std::uint16_t character_id=0;
    std::vector<FontGlyph> glyphs;
};
struct FontResult {
    Error error=Error::none;
    std::string message;
    FontAsset font;
    explicit operator bool() const noexcept {return error==Error::none;}
};
[[nodiscard]] FontResult decode_define_font(const Tag& tag,
    std::size_t max_glyphs=65535,std::size_t max_glyph_bytes=8*1024*1024);
}
