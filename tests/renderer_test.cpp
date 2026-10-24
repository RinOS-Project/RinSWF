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
    ShapeGeometry rectangle;
    rectangle.fills.push_back({{240,40,20,255}});
    rectangle.commands.push_back({ShapeCommandKind::move_to,{}, {20,20},0,0,0});
    rectangle.commands.push_back({ShapeCommandKind::line_to,{}, {60,20},0,1,0});
    rectangle.commands.push_back({ShapeCommandKind::line_to,{}, {60,60},0,1,0});
    rectangle.commands.push_back({ShapeCommandKind::line_to,{}, {20,60},0,1,0});
    rectangle.commands.push_back({ShapeCommandKind::line_to,{}, {20,20},0,1,0});
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,rectangle));
    const auto inside=(1*4+1)*4;
    assert(canvas.rgba[inside]==240 && canvas.rgba[inside+3]==255);
    assert(canvas.rgba[0]==0 && canvas.rgba[3]==0);
    ShapeFill gradient;
    gradient.kind=FillKind::linear_gradient;
    gradient.matrix.scale_x=0.001;
    gradient.matrix.scale_y=0.001;
    gradient.stops={ {0,{0,0,255,255}}, {255,{255,0,0,255}} };
    rectangle.fills[0]=gradient;
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,rectangle));
    assert(canvas.rgba[inside+3]==255);
    rectangle.fills[0].matrix.scale_x=0;
    assert(!draw_shape_fills(canvas,rectangle));
    rectangle.commands[1].fill1=2;
    assert(!draw_shape_fills(canvas,rectangle));
    assert(!make_surface(0,2,canvas));
    return 0;
}
