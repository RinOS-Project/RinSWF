// SPDX-License-Identifier: MIT
#include "rinswf/shapes.hpp"
#include <cassert>
using namespace rinswf;
int main() {
    // DefineShape: character ID 7, empty RECT, no styles, then EndShapeRecord.
    Tag empty{2,0,{7,0,0,0,0,0,0}};
    auto result=decode_shape(empty);
    assert(result);
    assert(result.shape.character_id==7);
    assert(result.shape.commands.empty());
    empty.data[3]=1; // unsupported fill-style array
    assert(!decode_shape(empty));
    assert(!decode_shape({9,0,{}}));
}
