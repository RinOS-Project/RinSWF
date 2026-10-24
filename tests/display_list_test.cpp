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
    d.tags[0].data={0x06,4,0,7,0};
    assert(!build_timeline(d)); // malformed MATRIX
    return 0;
}
