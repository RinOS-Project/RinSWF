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
struct BlurFilter {
    std::uint8_t radius_x=0,radius_y=0,passes=0;
};
struct GlowFilter {
    BlurFilter blur{};
    std::uint8_t red=0,green=0,blue=0,alpha=0;
};
struct DropShadowFilter {
    BlurFilter blur{};
    std::uint8_t red=0,green=0,blue=0,alpha=0;
    std::int16_t offset_x=0,offset_y=0; // whole output pixels
};
struct ColorMatrixFilter {
    float values[20]={1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,1,0};
};
struct BevelFilter {
    BlurFilter blur{};
    std::uint8_t shadow[4]={0,0,0,0};
    std::uint8_t highlight[4]={255,255,255,0};
    std::int16_t offset_x=0,offset_y=0;
};
struct GradientGlowFilter {
    BlurFilter blur{};
    std::vector<std::uint8_t> ratios;
    std::vector<std::uint8_t> rgba; // four bytes per stop
};
struct GradientBevelFilter {
    BlurFilter blur{};
    std::vector<std::uint8_t> ratios;
    std::vector<std::uint8_t> rgba;
    std::int16_t offset_x=0,offset_y=0;
};
struct FilterEffect {
    std::uint8_t type=1; // 0 DropShadow, 1 Blur, 2 Glow
    BlurFilter blur{};
    GlowFilter glow{};
    DropShadowFilter shadow{};
    ColorMatrixFilter color_matrix{};
    BevelFilter bevel{};
    GradientGlowFilter gradient_glow{};
    GradientBevelFilter gradient_bevel{};
};
struct DisplayEntry {
    std::uint16_t depth=0;
    std::uint16_t clip_depth=0; // nonzero: mask affects depths (depth,clip_depth]
    std::uint16_t character_id=0;
    std::size_t placed_frame=0; // zero-based parent frame when this instance was placed
    std::vector<FilterEffect> filters; // SWF FilterList order
    bool has_shadow=false;
    DropShadowFilter shadow{};
    bool has_glow=false;
    GlowFilter glow{};
    bool has_blur=false;
    BlurFilter blur{};
    std::uint8_t blend_mode=1; // SWF BlendMode: 1 normal, 3 multiply, 4 screen, 8 add
    bool has_color_transform=false;
    ColorTransform color_transform{};
    bool has_matrix=false;
    Matrix matrix{};
    std::string name;
};
struct FrameSnapshot {
    std::vector<DisplayEntry> objects; // sorted by depth
    std::vector<std::vector<std::uint8_t>> actions; // DoAction blocks in tag order
    std::vector<std::string> labels; // FrameLabel names for this frame
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
