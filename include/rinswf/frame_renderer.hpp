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
// Selects the root display-list snapshot from a built timeline, wrapping at
// its frame count. Sprite clocks restart with each root timeline loop.
// The caller clears the surface when it wants a fresh frame.
[[nodiscard]] RenderResult render_timeline_playback(const TimelineResult& timeline,
                                                    const FrameAssets& assets,
                                                    Surface& target,
                                                    const TimelinePlayback& playback,
                                                    StrokeLimits limits={});
// Opt-in per-root-depth Sprite controls. State is caller-owned, so multiple
// instances of the same character can be paused and seeked independently.
struct SpriteInstancePlayback {
    std::size_t frame=0;
    bool playing=true;
};
struct SpritePlayback {
    std::map<std::uint16_t,SpriteInstancePlayback> instances;
    // Tracks identity separately from explicit playback overrides.
    std::map<std::uint16_t,std::pair<std::uint16_t,std::size_t>> identities;
    void synchronize(const FrameSnapshot& frame,const FrameAssets& assets);
    void stop(std::uint16_t depth) {instances[depth].playing=false;}
    void play(std::uint16_t depth) {instances[depth].playing=true;}
    void seek(std::uint16_t depth,std::size_t frame) {instances[depth].frame=frame;}
    void advance(std::size_t count=1) noexcept;
    void erase(std::uint16_t depth) {instances.erase(depth); identities.erase(depth);}
};
[[nodiscard]] RenderResult render_timeline_playback(
    const TimelineResult& timeline,const FrameAssets& assets,Surface& target,
    const TimelinePlayback& playback,const SpritePlayback& sprites,
    StrokeLimits limits={});
// Stateful root player. Synchronizes sprite lifecycle before drawing and
// advances only explicitly requested frames. A failed draw leaves state intact.
struct TimelinePlayer {
    TimelinePlayback clock;
    SpritePlayback sprites;
    std::size_t last_cycle=0;
    bool initialized=false;
    void seek(std::size_t frame) noexcept {clock.seek(frame);}
    void pause() noexcept {clock.pause();}
    void resume() noexcept {clock.resume();}
    void advance(std::size_t count=1) noexcept {
        if(!clock.playing)return;
        clock.advance(count);
        sprites.advance(count);
    }
    [[nodiscard]] RenderResult render(const TimelineResult& timeline,
                                      const FrameAssets& assets,Surface& target,
                                      StrokeLimits limits={});
};
// Decodes supported shape/lossless/JPEG definitions. Unsupported placed
// characters and unsupported transforms fail during frame rendering.
[[nodiscard]] RenderResult collect_frame_assets(const Document& document,
                                                  FrameAssets& output,
                                                  std::size_t max_pixels=16*1024*1024);
}
