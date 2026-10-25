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
// Renders a snapshot at a zero-based playback frame; placed sprites use their placement offsets.
[[nodiscard]] RenderResult render_frame_at(const FrameSnapshot& frame,
                                           const FrameAssets& assets,Surface& target,
                                           std::size_t playback_frame,
                                           StrokeLimits limits={});
// Explicit root timeline playback controller. The caller owns its clock;
// advance() never runs hidden timers or executes ActionScript.
struct TimelinePlayback {
    std::size_t frame=0;
    bool playing=true;
    void pause() noexcept {playing=false;}
    void resume() noexcept {playing=true;}
    void seek(std::size_t next) noexcept {frame=next;}
    void advance(std::size_t count=1) noexcept;
};
[[nodiscard]] RenderResult render_playback(const FrameSnapshot& snapshot,
                                           const FrameAssets& assets,Surface& target,
                                           const TimelinePlayback& playback,
                                           StrokeLimits limits={});
// Decodes supported shape/lossless/JPEG definitions. Unsupported placed
// characters and unsupported transforms fail during frame rendering.
[[nodiscard]] RenderResult collect_frame_assets(const Document& document,
                                                  FrameAssets& output,
                                                  std::size_t max_pixels=16*1024*1024);
}
