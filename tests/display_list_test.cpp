// SPDX-License-Identifier: MIT
#include "rinswf/display_list.hpp"
#include <cassert>
using namespace rinswf;
int main() {
    Document d;
    d.header.frame_count=3;
    // PlaceObject2 with character ID 7, depth 4 and identity MATRIX.
    d.tags.push_back({26,0,{0x06,4,0,7,0,0x00}});
    d.tags.push_back({1,0,{}});
    d.tags.push_back({26,0,{0x21,4,0,'h','e','r','o',0}});
    d.tags.push_back({1,0,{}});
    d.tags.push_back({28,0,{4,0}});
    d.tags.push_back({1,0,{}});
    auto result=build_timeline(d);
    assert(result);
    assert(result.frames.size()==3);
    assert(result.frames[0].objects[0].character_id==7);
    assert(result.frames[1].objects[0].name=="hero");
    assert(result.frames[2].objects.empty());
    d.tags[0].data={0x06,4,0,7,0,0x0c,0xa6,0xc0}; // translation (20, -10) twips
    auto moved=build_timeline(d);
    assert(moved);
    assert(moved.frames[0].objects[0].matrix.translate_x==20);
    assert(moved.frames[0].objects[0].matrix.translate_y==-10);
    assert(moved.frames[1].objects[0].matrix.translate_x==20);
    d.tags[0].data={0x06,4,0,7,0};
    assert(!build_timeline(d)); // malformed MATRIX
    // DefineSprite (39) has its own independent ShowFrame and End tags.
    // Character 13 is placed at depth 2 inside sprite 9.
    Tag sprite;
    sprite.code=39;
    sprite.data={9,0,1,0, 0x86,0x06, 0x06,2,0,13,0,0x00,
                 0x40,0x00, 0x00,0x00};
    auto decoded=build_sprite_timeline(sprite);
    assert(decoded);
    assert(decoded.sprite.character_id==9);
    assert(decoded.sprite.frames.size()==1);
    assert(decoded.sprite.frames[0].objects.size()==1);
    assert(decoded.sprite.frames[0].objects[0].character_id==13);
    sprite.data.pop_back();
    assert(!build_sprite_timeline(sprite));
    sprite.data.push_back(0);
    sprite.data[2]=2; // declared frame count disagrees with ShowFrame
    assert(!build_sprite_timeline(sprite));
    sprite.data[2]=1;
    assert(!build_sprite_timeline(sprite,Limits{1024,0,1024}));
    // Moving a placed instance preserves its clock; replacing its character
    // starts a fresh instance on the current parent frame.
    Document instances;
    instances.header.frame_count=4;
    instances.tags.push_back({26,0,{0x06,1,0,7,0,0x00}});
    instances.tags.push_back({1,0,{}});
    instances.tags.push_back({26,0,{0x01,1,0}});
    instances.tags.push_back({1,0,{}});
    instances.tags.push_back({26,0,{0x03,1,0,8,0}});
    instances.tags.push_back({1,0,{}});
    instances.tags.push_back({1,0,{}});
    auto instance_frames=build_timeline(instances);
    assert(instance_frames);
    assert(instance_frames.frames[0].objects[0].placed_frame==0);
    assert(instance_frames.frames[1].objects[0].placed_frame==0);
    assert(instance_frames.frames[2].objects[0].placed_frame==2);
    assert(instance_frames.frames[3].objects[0].placed_frame==2);
    // PlaceObject3: Flags1, Flags2, Depth, CharacterId, MATRIX.
    Document place3;
    place3.header.frame_count=3;
    place3.tags.push_back({70,0,{0x06,0x00,2,0,17,0,0x00}});
    place3.tags.push_back({1,0,{}});
    // Move same instance and change its name without restarting its clock.
    place3.tags.push_back({70,0,{0x21,0x00,2,0,'c','l','i','p',0}});
    place3.tags.push_back({1,0,{}});
    // Character replacement restarts its local clock.
    place3.tags.push_back({70,0,{0x03,0x00,2,0,18,0}});
    place3.tags.push_back({1,0,{}});
    auto p3=build_timeline(place3);
    assert(p3);
    assert(p3.frames[0].objects[0].character_id==17);
    assert(p3.frames[1].objects[0].name=="clip");
    assert(p3.frames[1].objects[0].placed_frame==0);
    assert(p3.frames[2].objects[0].character_id==18);
    assert(p3.frames[2].objects[0].placed_frame==2);
    place3.tags[0].data[1]=0x02; // BlendMode cannot yet be rendered
    assert(!build_timeline(place3));
    place3.tags[0].data[1]=0;
    place3.tags[0].data.pop_back(); // truncated MATRIX
    assert(!build_timeline(place3));
    return 0;
}
