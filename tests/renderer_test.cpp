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
    Surface thick;
    assert(make_surface(12,12,thick));
    ShapeGeometry wide_line;
    wide_line.lines.push_back({100,{255,180,0,255}}); // five-pixel stroke
    wide_line.commands.push_back({ShapeCommandKind::move_to,{}, {40,100},0,0,0});
    wide_line.commands.push_back({ShapeCommandKind::line_to,{}, {180,100},0,0,1});
    assert(draw_shape_strokes(thick,wide_line));
    assert(thick.rgba[(3*12+5)*4+3]==255); // within thickness
    assert(thick.rgba[(0*12+5)*4+3]==0);   // outside thickness
    // A thin horizontal line near a pixel boundary should produce partial alpha.
    Surface aa;
    assert(make_surface(8,8,aa));
    ShapeGeometry thin;
    thin.lines.push_back({20,{250,20,20,255}});
    thin.commands.push_back({ShapeCommandKind::move_to,{}, {20,45},0,0,0});
    thin.commands.push_back({ShapeCommandKind::line_to,{}, {120,45},0,0,1});
    assert(draw_shape_strokes(aa,thin));
    const auto partial=aa.rgba[(2*8+3)*4+3];
    assert(partial>0 && partial<255);
    // LINESTYLE2 cap comparison: square extends beyond endpoint, butt does not.
    Surface caps;
    assert(make_surface(12,12,caps));
    ShapeGeometry cap_line;
    ShapeLine cap_style;
    cap_style.width=40;cap_style.color={255,255,0,255};
    cap_style.start_cap=1;cap_style.end_cap=1;
    cap_line.lines.push_back(cap_style);
    cap_line.commands.push_back({ShapeCommandKind::move_to,{}, {60,100},0,0,0});
    cap_line.commands.push_back({ShapeCommandKind::line_to,{}, {140,100},0,0,1});
    assert(draw_shape_strokes(caps,cap_line));
    assert(caps.rgba[(5*12+2)*4+3]==0);
    clear(caps,{0,0,0,0});
    cap_line.lines[0].start_cap=2;cap_line.lines[0].end_cap=2;
    assert(draw_shape_strokes(caps,cap_line));
    assert(caps.rgba[(5*12+2)*4+3]>0);
    ShapeGeometry joined;
    ShapeLine joined_style;
    joined_style.width=40;
    joined_style.color={180,40,240,255};
    joined_style.join=0; // round
    joined_style.start_cap=1;joined_style.end_cap=1;
    joined.lines.push_back(joined_style);
    joined.commands.push_back({ShapeCommandKind::move_to,{}, {40,80},0,0,0});
    joined.commands.push_back({ShapeCommandKind::line_to,{}, {80,80},0,0,1});
    joined.commands.push_back({ShapeCommandKind::line_to,{}, {80,120},0,0,1});
    clear(caps,{0,0,0,0});
    assert(draw_shape_strokes(caps,joined));
    assert(caps.rgba[(4*12+4)*4+3]>0);
    // Bevel and miter joins should handle corners without invalid memory access.
    joined.lines[0].join=1;
    clear(caps,{0,0,0,0});
    assert(draw_shape_strokes(caps,joined));
    joined.lines[0].join=2;
    joined.lines[0].miter_limit=3.0;
    clear(caps,{0,0,0,0});
    assert(draw_shape_strokes(caps,joined));
    joined.lines[0].miter_limit=1.0; // bevel fallback
    clear(caps,{0,0,0,0});
    assert(draw_shape_strokes(caps,joined));
    // Overlapping segments using the same style must not double-composite alpha.
    Surface overlap;
    assert(make_surface(8,8,overlap));
    ShapeGeometry translucent;
    translucent.lines.push_back({40,{255,0,0,128}});
    translucent.commands.push_back({ShapeCommandKind::move_to,{}, {20,60},0,0,0});
    translucent.commands.push_back({ShapeCommandKind::line_to,{}, {120,60},0,0,1});
    assert(draw_shape_strokes(overlap,translucent));
    const auto once=overlap.rgba[(3*8+3)*4+3];
    translucent.commands.push_back({ShapeCommandKind::move_to,{}, {20,60},0,0,0});
    translucent.commands.push_back({ShapeCommandKind::line_to,{}, {120,60},0,0,1});
    clear(overlap,{0,0,0,0});
    assert(draw_shape_strokes(overlap,translucent));
    assert(overlap.rgba[(3*8+3)*4+3]==once);
    // Mask opacity should apply source alpha once, and unused styles are valid.
    Surface alpha_canvas;
    assert(make_surface(8,8,alpha_canvas));
    ShapeGeometry one_style;
    one_style.lines.push_back({40,{220,30,20,128}});
    one_style.lines.push_back({40,{0,255,0,255}}); // unused
    one_style.commands.push_back({ShapeCommandKind::move_to,{}, {20,60},0,0,0});
    one_style.commands.push_back({ShapeCommandKind::line_to,{}, {120,60},0,0,1});
    assert(draw_shape_strokes(alpha_canvas,one_style));
    assert(alpha_canvas.rgba[(3*8+3)*4+3]==128);
    // A small stroke on a large surface must composite at the correct global offset.
    Surface large;
    assert(make_surface(256,256,large));
    ShapeGeometry local;
    local.lines.push_back({20,{75,150,225,255}});
    local.commands.push_back({ShapeCommandKind::move_to,{}, {3000,3000},0,0,0});
    local.commands.push_back({ShapeCommandKind::line_to,{}, {3200,3000},0,0,1});
    assert(draw_shape_strokes(large,local));
    assert(large.rgba[(150*256+155)*4+3]>0);
    assert(large.rgba[3]==0);
    // Sparse mask covers distant segments without allocating the intervening rectangle.
    Surface sparse;
    assert(make_surface(512,512,sparse));
    ShapeGeometry separated;
    separated.lines.push_back({20,{60,200,90,200}});
    separated.commands.push_back({ShapeCommandKind::move_to,{}, {100,100},0,0,0});
    separated.commands.push_back({ShapeCommandKind::line_to,{}, {200,100},0,0,1});
    separated.commands.push_back({ShapeCommandKind::move_to,{}, {9500,9500},0,0,0});
    separated.commands.push_back({ShapeCommandKind::line_to,{}, {9600,9500},0,0,1});
    assert(draw_shape_strokes(sparse,separated));
    assert(sparse.rgba[(5*512+7)*4+3]>0);
    assert(sparse.rgba[(475*512+477)*4+3]>0);
    assert(sparse.rgba[(256*512+256)*4+3]==0);
    // Explicit tile budgets: a drawing requiring two separate tiles must fail with one.
    Surface budget_surface;
    assert(make_surface(192,64,budget_surface));
    ShapeGeometry budget_shape;
    budget_shape.lines.push_back({20,{255,0,0,255}});
    budget_shape.commands.push_back({ShapeCommandKind::move_to,{}, {200,200},0,0,0});
    budget_shape.commands.push_back({ShapeCommandKind::line_to,{}, {3000,200},0,0,1});
    const auto before_budget=budget_surface.rgba;
    auto limited=draw_shape_strokes(budget_surface,budget_shape,0,0,{1,4096});
    assert(!limited && limited.error==Error::size_limit);
    assert(budget_surface.rgba==before_budget);
    assert(draw_shape_strokes(budget_surface,budget_shape,0,0,{4,16384}));
    // Closing edge of a one-style contour must not introduce external caps.
    Surface closed_canvas;
    assert(make_surface(16,16,closed_canvas));
    ShapeGeometry closed_shape;
    ShapeLine cs; cs.width=40; cs.color={100,180,250,255};
    cs.start_cap=2; cs.end_cap=2;
    closed_shape.lines.push_back(cs);
    closed_shape.commands.push_back({ShapeCommandKind::move_to,{}, {80,80},0,0,0});
    closed_shape.commands.push_back({ShapeCommandKind::line_to,{}, {200,80},0,0,1});
    closed_shape.commands.push_back({ShapeCommandKind::line_to,{}, {200,200},0,0,1});
    closed_shape.commands.push_back({ShapeCommandKind::line_to,{}, {80,80},0,0,1});
    assert(draw_shape_strokes(closed_canvas,closed_shape));
    assert(closed_canvas.rgba[(4*16+7)*4+3]>0);
    // Closed corners also need the final-to-first join, including miter fallback.
    closed_shape.lines[0].join=2;
    closed_shape.lines[0].miter_limit=2.0;
    clear(closed_canvas,{0,0,0,0});
    assert(draw_shape_strokes(closed_canvas,closed_shape));
    assert(closed_canvas.rgba[(4*16+4)*4+3]>0);
    closed_shape.lines[0].join=1;
    clear(closed_canvas,{0,0,0,0});
    assert(draw_shape_strokes(closed_canvas,closed_shape));
    // A quadratic closing edge joins its tangent to the first straight edge.
    ShapeGeometry curved_loop;
    ShapeLine curved_style;
    curved_style.width=40;
    curved_style.color={200,100,40,255};
    curved_style.join=2;
    curved_style.miter_limit=4.0;
    curved_loop.lines.push_back(curved_style);
    curved_loop.commands.push_back({ShapeCommandKind::move_to,{}, {80,80},0,0,0});
    curved_loop.commands.push_back({ShapeCommandKind::line_to,{}, {200,80},0,0,1});
    curved_loop.commands.push_back({ShapeCommandKind::line_to,{}, {200,200},0,0,1});
    curved_loop.commands.push_back({ShapeCommandKind::quadratic_to, {100,180}, {80,80},0,0,1});
    clear(closed_canvas,{0,0,0,0});
    assert(draw_shape_strokes(closed_canvas,curved_loop));
    assert(closed_canvas.rgba[(4*16+4)*4+3]>0);
    // An open quadratic-to-quadratic corner uses both endpoint tangents.
    ShapeGeometry double_curve;
    ShapeLine curve_line;
    curve_line.width=40;
    curve_line.color={250,80,60,255};
    curve_line.join=2;
    curve_line.miter_limit=3.0;
    double_curve.lines.push_back(curve_line);
    double_curve.commands.push_back({ShapeCommandKind::move_to,{}, {40,160},0,0,0});
    double_curve.commands.push_back({ShapeCommandKind::quadratic_to,{100,120},{160,160},0,0,1});
    double_curve.commands.push_back({ShapeCommandKind::quadratic_to,{200,200},{240,160},0,0,1});
    clear(closed_canvas,{0,0,0,0});
    assert(draw_shape_strokes(closed_canvas,double_curve));
    assert(closed_canvas.rgba[(8*16+8)*4+3]>0);
    // Two consecutive quadratic edges should be joined by their endpoint tangents.
    Surface quadratic_surface;
    assert(make_surface(32,32,quadratic_surface));
    ShapeGeometry quadratic_pair;
    ShapeLine tangent_style;
    tangent_style.width=40;
    tangent_style.color={150,90,200,255};
    tangent_style.join=2;
    tangent_style.miter_limit=3.0;
    quadratic_pair.lines.push_back(tangent_style);
    quadratic_pair.commands.push_back({ShapeCommandKind::move_to,{}, {100,300},0,0,0});
    quadratic_pair.commands.push_back({ShapeCommandKind::quadratic_to,{180,100},{300,300},0,0,1});
    quadratic_pair.commands.push_back({ShapeCommandKind::quadratic_to,{420,500},{500,300},0,0,1});
    assert(draw_shape_strokes(quadratic_surface,quadratic_pair));
    assert(quadratic_surface.rgba[(15*32+15)*4+3]>0);
    // Style changes must create independent stroke runs (no borrowed tangent).
    Surface switched;
    assert(make_surface(32,32,switched));
    ShapeGeometry alternating;
    ShapeLine red; red.width=40; red.color={255,0,0,255};
    ShapeLine blue=red; blue.color={0,0,255,255};
    alternating.lines.push_back(red);
    alternating.lines.push_back(blue);
    alternating.commands.push_back({ShapeCommandKind::move_to,{}, {100,100},0,0,0});
    alternating.commands.push_back({ShapeCommandKind::line_to,{}, {300,100},0,0,1});
    alternating.commands.push_back({ShapeCommandKind::line_to,{}, {300,300},0,0,2});
    alternating.commands.push_back({ShapeCommandKind::line_to,{}, {500,300},0,0,1});
    assert(draw_shape_strokes(switched,alternating));
    assert(switched.rgba[(5*32+10)*4]>0);
    assert(switched.rgba[(10*32+15)*4+2]>0);
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
    gradient.kind=FillKind::focal_gradient;
    gradient.focal_point=0.5;
    rectangle.fills[0]=gradient;
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,rectangle));
    assert(canvas.rgba[inside+3]==255);
    rectangle.fills[0].focal_point=1.5;
    assert(!draw_shape_fills(canvas,rectangle));
    gradient.kind=FillKind::linear_gradient;
    BitmapLibrary library;
    Bitmap tile;
    tile.character_id=42;tile.width=1;tile.height=1;
    tile.rgba={15,220,90,255};
    library.images.emplace(42,tile);
    ShapeFill tiled;
    tiled.kind=FillKind::repeating_bitmap;
    tiled.bitmap_id=42;
    tiled.matrix.scale_x=20;
    tiled.matrix.scale_y=20;
    rectangle.fills[0]=tiled;
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,rectangle,0,0,&library));
    assert(canvas.rgba[inside]==15 && canvas.rgba[inside+1]==220);
    // Smoothed repeating fill blends red and blue across their boundary.
    Bitmap pair;
    pair.character_id=43;pair.width=2;pair.height=1;
    pair.rgba={255,0,0,255,0,0,255,255};
    library.images.emplace(43,pair);
    rectangle.fills[0].bitmap_id=43;
    rectangle.fills[0].matrix.translate_x=10;
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,rectangle,0,0,&library));
    assert(canvas.rgba[inside]>0 && canvas.rgba[inside+2]>0);
    rectangle.fills[0].kind=FillKind::nonsmoothed_repeating_bitmap;
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,rectangle,0,0,&library));
    assert(canvas.rgba[inside]==0 && canvas.rgba[inside+2]==255);
    rectangle.fills[0].bitmap_id=99;
    assert(!draw_shape_fills(canvas,rectangle,0,0,&library));
    rectangle.fills[0]=gradient;
    rectangle.fills[0].matrix.scale_x=0;
    assert(!draw_shape_fills(canvas,rectangle));
    // Two overlapping contours with the same winding must retain the overlap.
    ShapeGeometry nested;
    nested.fills.push_back({{40,80,220,255}});
    auto contour=[&](int x0,int y0,int x1,int y1) {
        nested.commands.push_back({ShapeCommandKind::move_to,{}, {x0,y0},0,0,0});
        nested.commands.push_back({ShapeCommandKind::line_to,{}, {x1,y0},0,1,0});
        nested.commands.push_back({ShapeCommandKind::line_to,{}, {x1,y1},0,1,0});
        nested.commands.push_back({ShapeCommandKind::line_to,{}, {x0,y1},0,1,0});
        nested.commands.push_back({ShapeCommandKind::line_to,{}, {x0,y0},0,1,0});
    };
    contour(0,0,80,80);
    contour(20,20,60,60);
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,nested));
    assert(canvas.rgba[(1*4+1)*4+3]==255); // nonzero, not even-odd
    // A reversed inner contour cancels winding and forms a transparent hole.
    ShapeGeometry hole;
    hole.fills.push_back({{40,80,220,255}});
    auto add_edge=[&](int x,int y) {
        hole.commands.push_back({ShapeCommandKind::line_to,{}, {x,y},0,1,0});
    };
    hole.commands.push_back({ShapeCommandKind::move_to,{}, {0,0},0,0,0});
    add_edge(80,0);add_edge(80,80);add_edge(0,80);add_edge(0,0);
    hole.commands.push_back({ShapeCommandKind::move_to,{}, {20,20},0,0,0});
    add_edge(20,60);add_edge(60,60);add_edge(60,20);add_edge(20,20);
    clear(canvas,{0,0,0,0});
    assert(draw_shape_fills(canvas,hole));
    assert(canvas.rgba[(1*4+1)*4+3]==0);
    assert(canvas.rgba[(0*4+0)*4+3]==255);
    // A shape entirely outside the surface must not change the target.
    ShapeGeometry outside;
    outside.fills.push_back({{255,0,0,255}});
    outside.commands.push_back({ShapeCommandKind::move_to,{}, {200,200},0,0,0});
    outside.commands.push_back({ShapeCommandKind::line_to,{}, {260,200},0,1,0});
    outside.commands.push_back({ShapeCommandKind::line_to,{}, {260,260},0,1,0});
    outside.commands.push_back({ShapeCommandKind::line_to,{}, {200,260},0,1,0});
    outside.commands.push_back({ShapeCommandKind::line_to,{}, {200,200},0,1,0});
    const auto before_outside=canvas.rgba;
    assert(draw_shape_fills(canvas,outside));
    assert(canvas.rgba==before_outside);
    rectangle.commands[1].fill1=2;
    assert(!draw_shape_fills(canvas,rectangle));
    assert(!make_surface(0,2,canvas));
    return 0;
}
