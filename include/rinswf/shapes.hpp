// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <vector>
namespace rinswf {
struct ShapePoint { std::int32_t x=0, y=0; }; // twips
enum class ShapeCommandKind { move_to, line_to, quadratic_to };
struct ShapeCommand {
    ShapeCommandKind kind=ShapeCommandKind::move_to;
    ShapePoint control{}, endpoint{};
    std::uint32_t fill0=0, fill1=0, line=0;
};
struct ShapeColor { std::uint8_t r=0,g=0,b=0,a=255; };
struct ShapeFill { ShapeColor color; };
struct ShapeLine { std::uint16_t width=0; ShapeColor color; };
struct ShapeGeometry {
    std::uint16_t character_id=0;
    std::vector<ShapeCommand> commands;
    std::vector<ShapeFill> fills;
    std::vector<ShapeLine> lines;
};
struct ShapeResult {
    Error error=Error::none;
    std::string message;
    ShapeGeometry shape;
    explicit operator bool() const noexcept { return error==Error::none; }
};
// Supports solid RGB/RGBA fills and LINESTYLE (DefineShape 1/2/3).
// Unsupported gradient/bitmap fills, LINESTYLE2 and NewStyles fail explicitly.
[[nodiscard]] ShapeResult decode_shape(const Tag& tag, std::size_t max_commands=100000);
}
