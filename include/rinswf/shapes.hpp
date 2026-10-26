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
struct GradientStop { std::uint8_t ratio=0; ShapeColor color; };
struct ShapeMatrix {
    double scale_x=1.0, scale_y=1.0, skew_x=0.0, skew_y=0.0;
    std::int32_t translate_x=0, translate_y=0;
};
enum class FillKind { solid, linear_gradient, radial_gradient, focal_gradient, repeating_bitmap, clipped_bitmap, nonsmoothed_repeating_bitmap, nonsmoothed_clipped_bitmap };
struct ShapeFill {
    ShapeColor color;
    FillKind kind=FillKind::solid;
    ShapeMatrix matrix{};
    std::uint8_t spread=0, interpolation=0;
    std::vector<GradientStop> stops;
    double focal_point=0.0;
    std::uint16_t bitmap_id=0;
};
struct ShapeLine {
    std::uint16_t width=0;
    ShapeColor color;
    std::uint8_t start_cap=0, end_cap=0, join=0;
    bool no_hscale=false, no_vscale=false, pixel_hinting=false, no_close=false;
    double miter_limit=0.0;
};
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
// Supports solid and gradient fills in DefineShape 1/2/3/4.
// Bitmap fills retain the character reference and transform; bitmap decoding/rendering remains separate.
// LINESTYLE2 solid-color strokes and appended NewStyles arrays are parsed;
// LINESTYLE2 fill-strokes remain unsupported.
[[nodiscard]] ShapeResult decode_shape(const Tag& tag, std::size_t max_commands=100000);
}
