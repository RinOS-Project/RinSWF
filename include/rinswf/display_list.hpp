// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace rinswf {
struct Matrix {
    double scale_x=1.0, scale_y=1.0;
    double rotate_skew_0=0.0, rotate_skew_1=0.0;
    std::int32_t translate_x=0, translate_y=0; // SWF twips
};
struct ColorTransform {
    double multiply[4]={1,1,1,1};
    std::int32_t add[4]={0,0,0,0};
};
struct DisplayEntry {
    std::uint16_t depth=0;
    std::uint16_t character_id=0;
    std::size_t placed_frame=0; // zero-based parent frame when this instance was placed
    std::uint8_t blend_mode=1; // SWF BlendMode: 1 normal, 3 multiply, 4 screen, 8 add
    bool has_color_transform=false;
    ColorTransform color_transform{};
    bool has_matrix=false;
    Matrix matrix{};
    std::string name;
};
struct FrameSnapshot {
    std::vector<DisplayEntry> objects; // sorted by depth
};
struct TimelineResult {
    Error error=Error::none;
    std::string message;
    std::vector<FrameSnapshot> frames;
    explicit operator bool() const noexcept { return error==Error::none; }
};
// Decodes a DefineSprite (tag 39) into its own independent timeline.
// Nested DefineSprite definitions are not evaluated by this function.
// Input limits bound contained tags; an End tag and exact frame count are required.
struct SpriteTimeline {
    std::uint16_t character_id=0;
    std::vector<FrameSnapshot> frames;
};
struct SpriteTimelineResult {
    Error error=Error::none;
    std::string message;
    SpriteTimeline sprite;
    explicit operator bool() const noexcept {return error==Error::none;}
};
[[nodiscard]] SpriteTimelineResult build_sprite_timeline(const Tag& tag,
                                                          Limits limits={});
// Builds a display-list timeline without executing ActionScript.
// Unsupported placement fields fail explicitly rather than silently changing playback.
[[nodiscard]] TimelineResult build_timeline(const Document& document);
}
