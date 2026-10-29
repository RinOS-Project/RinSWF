// SPDX-License-Identifier: MIT
#include "rinswf/frame_actions.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <set>
#include <utility>
namespace rinswf {
Avm1TypedResult execute_nested_sprite_entry_actions(
    const FrameSnapshot& root_frame,const FrameAssets& assets,
    TimelinePlayer& player,SpriteActionState& state,Avm1Limits limits) {
    auto staged=state.nested_instances;
    auto ancestry=state.ancestry;
    auto controls=player.sprites;
    std::set<std::vector<std::uint16_t>> visible;
    Avm1TypedResult last;
    std::size_t visited=0;
    std::size_t remaining_actions=limits.max_actions;
    std::size_t remaining_bytes=limits.max_bytes;
    std::function<Avm1TypedResult(const FrameSnapshot&,
        const std::vector<std::uint16_t>&,std::size_t,unsigned,
        std::vector<std::pair<std::uint16_t,std::size_t>>)> walk;
    walk=[&](const FrameSnapshot& frame,const std::vector<std::uint16_t>& parent,
             std::size_t parent_frame,unsigned depth,
             std::vector<std::pair<std::uint16_t,std::size_t>> parents)->Avm1TypedResult {
        if(depth>32)return {Error::size_limit,"nested Sprite depth limit",{}};
        for(const auto& entry:frame.objects) {
            const auto sprite=assets.sprites.find(entry.character_id);
            if(sprite==assets.sprites.end())continue;
            if(sprite->second.frames.empty())
                return {Error::malformed_tag,"nested Sprite has no frames",{}};
            auto path=parent;
            path.push_back(entry.depth);
            parents.emplace_back(entry.character_id,entry.placed_frame);
            // Root-level Sprite actions are owned by the root dispatcher.
            // Descendant instances have independent execution state.
            const auto control=depth==0?controls.instances.find(entry.depth):
                controls.instances.end();
            const auto nested=controls.nested_instances.find(path);
            std::size_t local=parent_frame>=entry.placed_frame?
                parent_frame-entry.placed_frame:0;
            if(depth==0 && control!=controls.instances.end())local=control->second.frame;
            if(depth>0 && nested!=controls.nested_instances.end())local=nested->second.frame;
            const auto frame_index=local%sprite->second.frames.size();
            if(depth>0) {
                if(++visited>state.max_instances)
                    return {Error::size_limit,"nested Sprite instance limit",{}};
                visible.insert(path);
                auto [it,inserted]=staged.try_emplace(path);
                auto& instance=it->second;
                if(inserted||instance.character_id!=entry.character_id||
                   instance.placed_frame!=entry.placed_frame ||
                   ancestry[path]!=parents) {
                    instance=SpriteActionInstance{};
                    instance.character_id=entry.character_id;
                    instance.placed_frame=entry.placed_frame;
                    controls.nested_instances.erase(path);
                    ancestry[path]=parents;
                }
                if(!instance.entered || instance.last_frame!=local) {
                    const auto& actions=sprite->second.frames[frame_index].actions;
                    if(actions.size()>remaining_actions)
                        return {Error::size_limit,"nested Sprite action budget",{}};
                    remaining_actions-=actions.size();
                    Avm1Context variables=instance.context;
                    Avm1TimelineCommand command;
                    const auto current=controls.nested_instances.find(path);
                    command.playing=current==controls.nested_instances.end()?true:current->second.playing;
                    for(const auto& block:actions) {
                        if(block.size()>remaining_bytes)
                            return {Error::size_limit,"nested Sprite byte budget",{}};
                        remaining_bytes-=block.size();
                        auto result=execute_avm1_typed(block,variables,limits,&command);
                        if(!result)return result;
                        last=std::move(result);
                    }
                    if(command.has_label) {
                        bool found=false;
                        for(std::size_t n=0;n<sprite->second.frames.size();++n)
                            for(const auto& label:sprite->second.frames[n].labels)
                                if(label==command.label) {
                                    if(found)return {Error::malformed_tag,"ambiguous nested Sprite label",{}};
                                    command.frame=n;found=true;
                                }
                        if(!found)return {Error::malformed_tag,"unknown nested Sprite label",{}};
                        command.has_frame=true;
                    }
                    if(command.has_frame && command.frame>=sprite->second.frames.size())
                        return {Error::malformed_tag,"nested Sprite frame out of range",{}};
                    instance.context=std::move(variables);
                    instance.entered=true;
                    instance.last_frame=local;
                    if(command.has_frame)controls.seek_path(path,command.frame);
                    if(command.playing)controls.play_path(path);
                    else controls.stop_path(path);
                }
            }
            const auto result=walk(sprite->second.frames[frame_index],path,local,depth+1,parents);
            if(!result)return result;
        }
        return {};
    };
    auto result=walk(root_frame,{},player.clock.frame,0,{});
    if(!result)return result;
    for(auto it=staged.begin();it!=staged.end();) {
        if(!visible.contains(it->first)) {
            controls.nested_instances.erase(it->first);
            controls.nested_identities.erase(it->first);
            ancestry.erase(it->first);
            it=staged.erase(it);
        } else ++it;
    }
    state.nested_instances=std::move(staged);
    state.ancestry=std::move(ancestry);
    player.sprites=std::move(controls);
    return last;
}
}
