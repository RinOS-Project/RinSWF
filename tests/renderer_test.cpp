// SPDX-License-Identifier: MIT
#include "rinswf/renderer.hpp"
#include <cassert>
using namespace rinswf;
int main() {
    Surface canvas;
    assert(make_surface(4,4,canvas));
    clear(canvas,{0,0,0,0});
    Bitmap bmp;
    bmp.width=1;bmp.height=1;bmp.rgba={255,0,0,255};
    assert(blit_bitmap(canvas,bmp,2,2));
    assert(canvas.rgba[(2*4+2)*4]==255);
    ShapeGeometry shape;
    shape.lines.push_back({20,{0,255,0,255}});
    shape.commands.push_back({ShapeCommandKind::move_to,{}, {0,0},0,0,0});
    shape.commands.push_back({ShapeCommandKind::line_to,{}, {60,0},0,0,1});
    assert(draw_shape_strokes(canvas,shape));
    assert(canvas.rgba[1]==255);
    assert(!make_surface(0,2,canvas));
    return 0;
}
