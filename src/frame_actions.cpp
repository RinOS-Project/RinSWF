// SPDX-License-Identifier: MIT
#include "rinswf/frame_actions.hpp"
#include <limits>
#include <utility>
#include <algorithm>
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
    if(command.has_label) {
        bool found=false;
        for(std::size_t i=0;i<timeline.frames.size();++i) {
            for(const auto& name:timeline.frames[i].labels) {
                if(name!=command.label)continue;
                if(found)return {Error::malformed_tag,"ambiguous frame label",{}};
                command.frame=i;
                found=true;
            }
        }
        if(!found)return {Error::malformed_tag,"unknown frame label",{}};
        command.has_frame=true;
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

Avm1TypedResult execute_frame_entry_actions(
    const TimelineResult& timeline,TimelinePlayer& player,
    Avm1Context& context,FrameActionState& state,Avm1Limits limits) {
    if(!timeline || timeline.frames.empty())
        return {Error::malformed_tag,"invalid root timeline",{}};
    const std::size_t source_frame=player.clock.frame;
    if(state.initialized && state.entered_frame==source_frame)
        return {};
    // The action executor is transactional. A failed entry leaves the
    // bookkeeping untouched so callers can report or retry the error.
    auto result=execute_frame_actions(timeline,player,context,limits);
    if(!result)return result;
    // Record the frame that actually executed. If script navigated away,
    // the destination is eligible for execution on the next explicit call.
    state.initialized=true;
    state.entered_frame=source_frame;
    return result;
}

Avm1TypedResult advance_root_actions(
    const TimelineResult& timeline,TimelinePlayer& player,
    Avm1Context& context,FrameActionState& state,
    std::size_t ticks,std::size_t max_ticks,Avm1Limits limits) {
    if(!timeline || timeline.frames.empty())
        return {Error::malformed_tag,"invalid root timeline",{}};
    if(ticks>max_ticks)
        return {Error::size_limit,"root timeline tick limit",{}};
    TimelinePlayer staged_player=player;
    Avm1Context staged_context=context;
    FrameActionState staged_state=state;
    Avm1TypedResult last;
    // Execute the initial frame once before advancing, even if stopped.
    // Each advanced tick has its own ShowFrame/DoAction entry point.
    last=execute_frame_entry_actions(timeline,staged_player,
                                     staged_context,staged_state,limits);
    if(!last)return last;
    for(std::size_t tick=0;tick<ticks && staged_player.clock.playing;++tick) {
        const auto old=staged_player.clock.frame;
        staged_player.advance(1);
        if(staged_player.clock.frame==old)
            return {Error::size_limit,"root timeline clock saturated",{}};
        last=execute_frame_entry_actions(timeline,staged_player,
                                         staged_context,staged_state,limits);
        if(!last)return last;
    }
    player=std::move(staged_player);
    context=std::move(staged_context);
    state=staged_state;
    return last;
}

namespace {
std::size_t value_bytes(const Avm1Value& value) {
    const auto* str=std::get_if<std::string>(&value);
    return str?sizeof(Avm1Value)+str->size():sizeof(Avm1Value);
}
bool charge_context(const Avm1Context& context,std::size_t& remaining) {
    for(const auto& [key,value]:context.variables) {
        const auto bytes=sizeof(key)+key.size()+value_bytes(value);
        if(bytes>remaining)return false;
        remaining-=bytes;
    }
    return true;
}
bool within_aggregate_memory(const Avm1Context& root,
                             const SpriteActionState& sprites,
                             std::size_t limit) {
    if(!charge_context(root,limit))return false;
    for(const auto& [depth,instance]:sprites.instances) {
        (void)depth;
        if(!charge_context(instance.context,limit))return false;
    }
    for(const auto& [path,instance]:sprites.nested_instances) {
        (void)path;
        if(!charge_context(instance.context,limit))return false;
    }
    return true;
}
}
Avm1TypedResult advance_timeline_actions(
    const TimelineResult& timeline,const FrameAssets& assets,
    TimelinePlayer& player,Avm1Context& context,
    FrameActionState& root_state,SpriteActionState& sprite_state,
    std::size_t ticks,std::size_t max_ticks,Avm1Limits limits) {
    if(!timeline || timeline.frames.empty())
        return {Error::malformed_tag,"invalid root timeline",{}};
    if(ticks>max_ticks)return {Error::size_limit,"timeline tick limit",{}};
    Avm1ExecutionBudget budget{limits.max_actions};
    limits.shared_budget=&budget;
    auto staged_player=player;
    auto staged_context=context;
    auto staged_root=root_state;
    auto staged_sprites=sprite_state;
    Avm1TypedResult last;
    auto enter=[&]()->Avm1TypedResult {
        auto result=execute_frame_entry_actions(timeline,staged_player,
                                                  staged_context,staged_root,limits);
        if(!result)return result;
        const auto& frame=timeline.frames[
            staged_player.clock.frame%timeline.frames.size()];
        result=execute_sprite_entry_actions(frame,assets,staged_player,
                                             staged_sprites,limits);
        if(!result)return result;
        result=execute_nested_sprite_entry_actions(frame,assets,staged_player,
                                                    staged_sprites,limits);
        if(!result)return result;
        if(!within_aggregate_memory(staged_context,staged_sprites,limits.max_value_bytes))
            return {Error::size_limit,"aggregate AVM1 retained value budget",{}};
        return result;
    };
    last=enter();
    if(!last)return last;
    for(std::size_t i=0;i<ticks && staged_player.clock.playing;++i) {
        const auto previous=staged_player.clock.frame;
        staged_player.advance(1);
        if(previous==staged_player.clock.frame)
            return {Error::size_limit,"timeline clock saturated",{}};
        last=enter();
        if(!last)return last;
    }
    player=std::move(staged_player);
    context=std::move(staged_context);
    root_state=staged_root;
    sprite_state=std::move(staged_sprites);
    return last;
}

Avm1TypedResult execute_sprite_entry_actions(
    const FrameSnapshot& root_frame,const FrameAssets& assets,
    TimelinePlayer& player,SpriteActionState& state,Avm1Limits limits) {
    auto updated=state;
    auto controls=player.sprites;
    // Root-depth identity is (depth, character, placement frame).
    // Purge removed instances before executing any actions.
    for(auto it=updated.instances.begin();it!=updated.instances.end();) {
        const auto found=std::find_if(root_frame.objects.begin(),root_frame.objects.end(),
            [&](const DisplayEntry& e){return e.depth==it->first &&
                e.character_id==it->second.character_id &&
                e.placed_frame==it->second.placed_frame;});
        if(found==root_frame.objects.end())it=updated.instances.erase(it);
        else ++it;
    }
    Avm1TypedResult last;
    for(const auto& entry:root_frame.objects) {
        const auto sprite=assets.sprites.find(entry.character_id);
        if(sprite==assets.sprites.end())continue;
        if(sprite->second.frames.empty())
            return {Error::malformed_tag,"empty sprite timeline",{}};
        auto it=updated.instances.find(entry.depth);
        if(it==updated.instances.end() ||
           it->second.character_id!=entry.character_id ||
           it->second.placed_frame!=entry.placed_frame) {
            if(it==updated.instances.end() &&
               updated.instances.size()>=updated.max_instances)
                return {Error::size_limit,"Sprite AVM1 instance limit",{}};
            SpriteActionInstance fresh;
            fresh.character_id=entry.character_id;
            fresh.placed_frame=entry.placed_frame;
            it=updated.instances.insert_or_assign(entry.depth,std::move(fresh)).first;
        }
        auto& instance=it->second;
        const auto override=controls.instances.find(entry.depth);
        const std::size_t selected=override!=controls.instances.end()?
            override->second.frame :
            (player.clock.frame>=entry.placed_frame?
             player.clock.frame-entry.placed_frame:0);
        const auto frame_index=selected%sprite->second.frames.size();
        if(instance.entered && instance.last_frame==selected)continue;
        const auto& frame=sprite->second.frames[frame_index];
        std::size_t total=0;
        if(frame.actions.size()>limits.max_actions)
            return {Error::size_limit,"Sprite AVM1 action block limit",{}};
        for(const auto& block:frame.actions) {
            if(total>limits.max_bytes || block.size()>limits.max_bytes-total)
                return {Error::size_limit,"Sprite AVM1 byte budget",{}};
            total+=block.size();
        }
        Avm1Context staged=instance.context;
        Avm1TimelineCommand command;
        command.playing=override==controls.instances.end()?true:override->second.playing;
        for(const auto& block:frame.actions) {
            last=execute_avm1_typed(block,staged,limits,&command);
            if(!last)return last;
        }
        if(command.has_label) {
            bool found=false;
            for(std::size_t n=0;n<sprite->second.frames.size();++n)
                for(const auto& name:sprite->second.frames[n].labels)
                    if(name==command.label) {
                        if(found)return {Error::malformed_tag,"ambiguous Sprite label",{}};
                        found=true;command.frame=n;
                    }
            if(!found)return {Error::malformed_tag,"unknown Sprite label",{}};
            command.has_frame=true;
        }
        if(command.has_frame && command.frame>=sprite->second.frames.size())
            return {Error::malformed_tag,"Sprite GotoFrame outside timeline",{}};
        instance.context=std::move(staged);
        instance.entered=true;
        instance.last_frame=selected;
        if(command.has_frame || command.sets_playing) {
            // Preserve implicit elapsed-frame playback until a script actually
            // changes the clock. Explicit Stop must freeze the current frame.
            if(controls.instances.find(entry.depth)==controls.instances.end())
                controls.seek(entry.depth,selected);
            if(command.has_frame)controls.seek(entry.depth,command.frame);
            if(command.sets_playing) {
                if(command.playing)controls.play(entry.depth);
                else controls.stop(entry.depth);
            }
        }
    }
    state=std::move(updated);
    player.sprites=std::move(controls);
    return last;
}

}
