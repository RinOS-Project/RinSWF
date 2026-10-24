// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/display_list.hpp"
#include "rinswf/renderer.hpp"
#include <map>
namespace rinswf {
struct FrameAssets {
    std::map<std::uint16_t,ShapeGeometry> shapes;
    BitmapLibrary bitmaps;
};
[[nodiscard]] RenderResult render_frame(const FrameSnapshot& frame,
                                        const FrameAssets& assets,Surface& target,
                                        StrokeLimits limits={});
// Decodes supported shape/lossless/JPEG definitions. Unsupported placed
// characters and unsupported transforms fail during frame rendering.
[[nodiscard]] RenderResult collect_frame_assets(const Document& document,
                                                  FrameAssets& output,
                                                  std::size_t max_pixels=16*1024*1024);
}
