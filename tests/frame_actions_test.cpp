// SPDX-License-Identifier: MIT
#include "rinswf/frame_actions.hpp"
#include <cstdint>
#include <vector>
int main() {
    rinswf::FrameSnapshot frame;
    // Push "score" and 42, then SetVariable and End.
    frame.actions.push_back({0x96,7,0,0,'s','c','o','r','e',0,
                             0x96,5,0,7,42,0,0,0,0x1d,0});
    rinswf::Avm1Context context;
    auto ok=rinswf::execute_frame_actions(frame,context);
    if(!ok || !context.variables.contains("score") ||
       !std::holds_alternative<double>(context.variables.at("score")) ||
       std::get<double>(context.variables.at("score"))!=42)return 1;
    // The second block is unsupported. No earlier mutation may leak out.
    frame.actions.push_back({0x04,0});
    context.variables.clear();
    if(rinswf::execute_frame_actions(frame,context))return 2;
    if(!context.variables.empty())return 3;
    if(rinswf::execute_frame_actions(frame,context,{100,1024,5}))return 4;
    return 0;
}
