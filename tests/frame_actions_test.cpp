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
    timeline.frames[2].actions.clear();
    timeline.frames[1].labels.push_back("intro");
    // AVM1 GoToLabel payload is the null-terminated label name.
    timeline.frames[2].actions.push_back({0x8c,6,0,'i','n','t','r','o',0,0});
    if(!rinswf::execute_frame_actions(timeline,player,context) ||
       player.clock.frame!=1)return 10;
    player.seek(2);
    timeline.frames[2].actions.clear();
    timeline.frames[2].actions.push_back({0x8c,8,0,'m','i','s','s','i','n','g',0,0});
    if(rinswf::execute_frame_actions(timeline,player,context) ||
       player.clock.frame!=2)return 11;
    // GotoFrame2 with numeric Push 2 (one-based), play flag set.
    player.seek(2);player.pause();
    timeline.frames[2].actions.clear();
    timeline.frames[2].actions.push_back({
        0x96,5,0,7,2,0,0,0,0x9f,1,0,1,0
    });
    if(!rinswf::execute_frame_actions(timeline,player,context) ||
       player.clock.frame!=1 || !player.clock.playing)return 12;
    // GotoFrame2 label, with Stop flag.
    player.seek(2);
    timeline.frames[2].actions.clear();
    timeline.frames[2].actions.push_back({
        0x96,7,0,0,'i','n','t','r','o',0,0x9f,1,0,0,0
    });
    if(!rinswf::execute_frame_actions(timeline,player,context) ||
       player.clock.frame!=1 || player.clock.playing)return 13;
    // Scene-bias forms must fail without modifying playback.
    player.seek(2);
    timeline.frames[2].actions.clear();
    timeline.frames[2].actions.push_back({
        0x96,5,0,7,1,0,0,0,0x9f,1,0,2,0
    });
    if(rinswf::execute_frame_actions(timeline,player,context) ||
       player.clock.frame!=2 || player.clock.playing)return 14;
    rinswf::TimelineResult entries;
    entries.frames.resize(2);
    entries.frames[0].actions.push_back({0x81,2,0,1,0,0});
    entries.frames[1].actions.push_back({0x07,0});
    rinswf::TimelinePlayer entry_player;
    rinswf::Avm1Context entry_context;
    rinswf::FrameActionState entry_state;
    if(!rinswf::execute_frame_entry_actions(entries,entry_player,
        entry_context,entry_state) || entry_player.clock.frame!=1)return 15;
    if(!rinswf::execute_frame_entry_actions(entries,entry_player,
        entry_context,entry_state) || entry_player.clock.playing)return 16;
    // Repeated entry at the same stopped frame must not execute again.
    entries.frames[1].actions={{0x04,0}};
    if(!rinswf::execute_frame_entry_actions(entries,entry_player,
        entry_context,entry_state))return 17;
    entry_state.reset();
    if(rinswf::execute_frame_entry_actions(entries,entry_player,
        entry_context,entry_state))return 18;
    rinswf::FrameAssets sprite_assets;
    rinswf::SpriteTimeline clip;
    clip.character_id=99;
    clip.frames.resize(2);
    clip.frames[0].actions.push_back({0x07,0});
    sprite_assets.sprites.emplace(99,clip);
    rinswf::FrameSnapshot root;
    rinswf::DisplayEntry placed;
    placed.depth=5;placed.character_id=99;placed.placed_frame=0;
    root.objects.push_back(placed);
    rinswf::TimelinePlayer sprite_player;
    rinswf::SpriteActionState sprite_state;
    if(!rinswf::execute_sprite_entry_actions(root,sprite_assets,
        sprite_player,sprite_state))return 19;
    if(sprite_player.sprites.instances.at(5).playing)return 20;
    sprite_assets.sprites.at(99).frames[0].actions={{0x04,0}};
    if(!rinswf::execute_sprite_entry_actions(root,sprite_assets,
        sprite_player,sprite_state))return 21;
    placed.character_id=100;
    root.objects[0]=placed;
    if(!rinswf::execute_sprite_entry_actions(root,sprite_assets,
        sprite_player,sprite_state))return 22;
    if(!sprite_state.instances.empty())return 23;
    return 0;
}
