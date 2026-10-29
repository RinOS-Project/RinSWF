// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/avm1.hpp"
#include "rinswf/display_list.hpp"
#include "rinswf/frame_renderer.hpp"
namespace rinswf {
// Opt-in execution: no rendering API runs SWF actions implicitly.
// All variable mutations across a frame are committed only if every block
// succeeds. Control-flow and host capabilities remain explicitly unsupported.
[[nodiscard]] Avm1TypedResult execute_frame_actions(
    const FrameSnapshot& frame,Avm1Context& context,Avm1Limits limits={});
// Opt-in root timeline control. A failed block preserves both player and vars.
[[nodiscard]] Avm1TypedResult execute_frame_actions(
    const TimelineResult& timeline,TimelinePlayer& player,
    Avm1Context& context,Avm1Limits limits={});
}
