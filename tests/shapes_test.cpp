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
    // DefineShape1: one solid red fill and one blue LINESTYLE.
    Tag styled{2,0,{9,0,0,1,0,255,0,0,1,20,0,0,0,255,0x11,0}};
    auto parsed=decode_shape(styled);
    assert(parsed);
    assert(parsed.shape.fills.size()==1);
    assert(parsed.shape.fills[0].color.r==255);
    assert(parsed.shape.lines.size()==1);
    assert(parsed.shape.lines[0].width==20);
    assert(parsed.shape.lines[0].color.b==255);
    // One-stop linear gradient with identity MATRIX and red stop.
    Tag gradient{2,0,{10,0,0,1,0x10,0,0x01,0,255,0,0,0,0,0}};
    auto grad=decode_shape(gradient);
    assert(grad);
    assert(grad.shape.fills.size()==1);
    assert(grad.shape.fills[0].kind==FillKind::linear_gradient);
    assert(grad.shape.fills[0].stops.size()==1);
    assert(grad.shape.fills[0].stops[0].color.r==255);
    // Bitmap fill references character 42 and carries its own identity matrix.
    Tag bitmap{2,0,{11,0,0,1,0x40,42,0,0,0,0,0}};
    auto bmp=decode_shape(bitmap);
    assert(bmp);
    assert(bmp.shape.fills.size()==1);
    assert(bmp.shape.fills[0].bitmap_id==42);
    assert(bmp.shape.fills[0].kind==FillKind::repeating_bitmap);
    bitmap.data[4]=0x43;
    auto clipped=decode_shape(bitmap);
    assert(clipped && clipped.shape.fills[0].kind==FillKind::nonsmoothed_clipped_bitmap);
    bitmap.data.resize(6);
    assert(!decode_shape(bitmap));
    styled.data[4]=0x7f; // unsupported fill type
    assert(!decode_shape(styled));
}
