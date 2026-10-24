// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace rinswf {
struct DisplayEntry {
    std::uint16_t depth=0;
    std::uint16_t character_id=0;
    bool has_matrix=false;
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
