// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/shapes.hpp"
#include "rinswf/swf.hpp"
#include <vector>
namespace rinswf {
struct TextGlyph {
    std::uint16_t font_id=0;
    std::uint32_t glyph_index=0;
    std::int32_t x=0,y=0; // twips
    std::uint16_t height=0;
    ShapeColor color{};
};
struct TextAsset {
    std::uint16_t character_id=0;
    ShapeMatrix matrix{};
    std::vector<TextGlyph> glyphs;
};
struct TextResult {
    Error error=Error::none;
    std::string message;
    TextAsset text;
    explicit operator bool() const noexcept {return error==Error::none;}
};
[[nodiscard]] TextResult decode_define_text(const Tag& tag,
    std::size_t max_glyphs=100000);
}
