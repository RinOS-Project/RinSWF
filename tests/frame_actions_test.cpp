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
    rinswf::TimelineResult timeline;
    timeline.frames.resize(3);
    // GotoFrame(2), Stop, End: root frame becomes index 2 and pauses.
    timeline.frames[0].actions.push_back({0x81,2,0,2,0,0x07,0});
    rinswf::TimelinePlayer player;
    auto moved=rinswf::execute_frame_actions(timeline,player,context);
    if(!moved || player.clock.frame!=2 || player.clock.playing)return 7;
    // A failing action must not commit an earlier Play or GotoFrame.
    timeline.frames[2].actions.push_back({0x06,0x81,2,0,1,0,0x04,0});
    auto rejected=rinswf::execute_frame_actions(timeline,player,context);
    if(rejected || player.clock.frame!=2 || player.clock.playing)return 8;
    // No out-of-range frame targets may be committed.
    timeline.frames[2].actions.clear();
    timeline.frames[2].actions.push_back({0x81,2,0,9,0,0});
    if(rinswf::execute_frame_actions(timeline,player,context) ||
       player.clock.frame!=2 || player.clock.playing)return 9;
    return 0;
}
