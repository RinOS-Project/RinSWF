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
struct DisplayEntry {
    std::uint16_t depth=0;
    std::uint16_t character_id=0;
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
// Builds a display-list timeline without executing ActionScript.
// Unsupported placement fields fail explicitly rather than silently changing playback.
[[nodiscard]] TimelineResult build_timeline(const Document& document);
}
