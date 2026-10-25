// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/display_list.hpp"
#include "rinswf/renderer.hpp"
#include <map>
namespace rinswf {
struct FrameAssets {
    std::map<std::uint16_t,ShapeGeometry> shapes;
    BitmapLibrary bitmaps;
    std::map<std::uint16_t,SpriteTimeline> sprites;
};
[[nodiscard]] RenderResult render_frame(const FrameSnapshot& frame,
                                        const FrameAssets& assets,Surface& target,
                                        StrokeLimits limits={});
// Renders a snapshot at a specified zero-based playback frame.
// Sprites advance with the parent clock and wrap at their frame count.
// Instance-specific start/reset times are not yet tracked.
[[nodiscard]] RenderResult render_frame_at(const FrameSnapshot& frame,
                                           const FrameAssets& assets,Surface& target,
                                           std::size_t playback_frame,
                                           StrokeLimits limits={});
// Decodes supported shape/lossless/JPEG definitions. Unsupported placed
// characters and unsupported transforms fail during frame rendering.
[[nodiscard]] RenderResult collect_frame_assets(const Document& document,
                                                  FrameAssets& output,
                                                  std::size_t max_pixels=16*1024*1024);
}
