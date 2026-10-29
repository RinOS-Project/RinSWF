// SPDX-License-Identifier: MIT
#include "rinswf/frame_actions.hpp"
#include <limits>
#include <utility>
namespace rinswf {
Avm1TypedResult execute_frame_actions(const FrameSnapshot& frame,
                                     Avm1Context& context,Avm1Limits limits) {
    if(frame.actions.size()>limits.max_actions)
        return {Error::size_limit,"frame action block limit",{}};
    std::size_t total_bytes=0;
    for(const auto& block:frame.actions) {
        if(block.size()>limits.max_bytes-total_bytes)
            return {Error::size_limit,"frame action byte budget",{}};
        total_bytes+=block.size();
    }
    Avm1Context staged=context;
    Avm1TypedResult last;
    for(const auto& block:frame.actions) {
        last=execute_avm1_typed(block,staged,limits);
        if(!last)return last;
    }
    context.variables=std::move(staged.variables);
    return last;
}
Avm1TypedResult execute_frame_actions(const TimelineResult& timeline,
                                     TimelinePlayer& player,Avm1Context& context,
                                     Avm1Limits limits) {
    if(!timeline || timeline.frames.empty())
        return {Error::malformed_tag,"invalid root timeline",{}};
    const auto current=player.clock.frame%timeline.frames.size();
    const auto& frame=timeline.frames[current];
    if(frame.actions.size()>limits.max_actions)
        return {Error::size_limit,"frame action block limit",{}};
    std::size_t bytes=0;
    for(const auto& block:frame.actions) {
        if(bytes>limits.max_bytes || block.size()>limits.max_bytes-bytes)
            return {Error::size_limit,"frame action byte budget",{}};
        bytes+=block.size();
    }
    Avm1Context staged=context;
    Avm1TimelineCommand command;
    command.playing=player.clock.playing;
    Avm1TypedResult last;
    for(const auto& block:frame.actions) {
        last=execute_avm1_typed(block,staged,limits,&command);
        if(!last)return last;
    }
    if(command.has_frame && command.frame>=timeline.frames.size())
        return {Error::malformed_tag,"GotoFrame outside root timeline",{}};
    // Both the variable environment and playback change only after success.
    context.variables=std::move(staged.variables);
    if(command.has_frame)player.seek(command.frame);
    if(command.playing)player.resume();
    else player.pause();
    return last;
}

}
