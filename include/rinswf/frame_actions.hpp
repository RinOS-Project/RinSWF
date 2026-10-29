// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/avm1.hpp"
#include "rinswf/display_list.hpp"
#include "rinswf/frame_renderer.hpp"
namespace rinswf {
struct SpriteActionInstance {
    std::uint16_t character_id=0;
    std::size_t placed_frame=0;
    Avm1Context context;
    bool entered=false;
    std::size_t last_frame=0;
};
struct SpriteActionState {
    std::map<std::uint16_t,SpriteActionInstance> instances;
    std::map<std::vector<std::uint16_t>,SpriteActionInstance> nested_instances;
    std::map<std::vector<std::uint16_t>,std::vector<std::pair<std::uint16_t,std::size_t>>> ancestry;
    std::size_t max_instances=256;
};
struct FrameActionState {
    bool initialized=false;
    std::size_t entered_frame=0;
    void reset() noexcept {initialized=false;entered_frame=0;}
};
// Opt-in execution: no rendering API runs SWF actions implicitly.
// All variable mutations across a frame are committed only if every block
// succeeds. Control-flow and host capabilities remain explicitly unsupported.
[[nodiscard]] Avm1TypedResult execute_frame_actions(
    const FrameSnapshot& frame,Avm1Context& context,Avm1Limits limits={});
// Opt-in root timeline control. A failed block preserves both player and vars.
[[nodiscard]] Avm1TypedResult execute_frame_actions(
    const TimelineResult& timeline,TimelinePlayer& player,
    Avm1Context& context,Avm1Limits limits={});
// Advances one logical root tick at a time and executes DoAction for each
// entered frame. Stops immediately when AVM1 Stop is encountered. The whole
// requested batch is committed only when all entered frames succeed.
// max_ticks limits work independently from per-script AVM1 budgets.
[[nodiscard]] Avm1TypedResult advance_root_actions(
    const TimelineResult& timeline,TimelinePlayer& player,
    Avm1Context& context,FrameActionState& state,
    std::size_t ticks,std::size_t max_ticks=1024,Avm1Limits limits={});
// Integrated opt-in ticking: root DoAction, root-depth Sprite DoAction,
// then nested Sprite DoAction at each logical frame. All state is committed
// together; rendering remains free of implicit script execution.
[[nodiscard]] Avm1TypedResult advance_timeline_actions(
    const TimelineResult& timeline,const FrameAssets& assets,
    TimelinePlayer& player,Avm1Context& context,
    FrameActionState& root_state,SpriteActionState& sprite_state,
    std::size_t ticks,std::size_t max_ticks=1024,Avm1Limits limits={});
// Execute actions only when the selected root frame changes. Call reset()
// after an explicit restart/seek that should re-enter the same frame.
[[nodiscard]] Avm1TypedResult execute_frame_entry_actions(
    const TimelineResult& timeline,TimelinePlayer& player,
    Avm1Context& context,FrameActionState& state,Avm1Limits limits={});
 // Explicit root-depth Sprite action dispatch; instance identity changes
 // discard old variables and frame-entry state.
[[nodiscard]] Avm1TypedResult execute_sprite_entry_actions(
    const FrameSnapshot& root_frame,const FrameAssets& assets,
    TimelinePlayer& player,SpriteActionState& state,Avm1Limits limits={});
// Opt-in nested dispatch. The depth path uniquely identifies a Sprite instance.
[[nodiscard]] Avm1TypedResult execute_nested_sprite_entry_actions(
    const FrameSnapshot& root_frame,const FrameAssets& assets,
    TimelinePlayer& player,SpriteActionState& state,Avm1Limits limits={});
}
