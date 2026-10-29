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
}
