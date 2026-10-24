// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/bitmaps.hpp"
#include "rinswf/shapes.hpp"
#include <cstdint>
#include <vector>
namespace rinswf {
struct Surface {
    std::uint32_t width=0,height=0;
    std::vector<std::uint8_t> rgba; // straight-alpha
};
struct RenderResult {
    Error error=Error::none;
    std::string message;
    explicit operator bool() const noexcept {return error==Error::none;}
};
[[nodiscard]] RenderResult make_surface(std::uint32_t width,std::uint32_t height,Surface& output,
                                        std::size_t max_pixels=16*1024*1024);
void clear(Surface& surface,ShapeColor color) noexcept;
[[nodiscard]] RenderResult blit_bitmap(Surface& target,const Bitmap& bitmap,
                                       std::int32_t x,std::int32_t y);
[[nodiscard]] RenderResult draw_shape_strokes(Surface& target,const ShapeGeometry& shape,
                                               std::int32_t offset_x=0,std::int32_t offset_y=0);
}
