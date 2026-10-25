// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include <cassert>
#include <limits>
int main() {
    using namespace rinswf;
    Surface target;
    assert(make_surface(8,8,target));
    FrameAssets assets;
    Bitmap image; image.character_id=42;image.width=1;image.height=1;
    image.rgba={255,0,0,255};
    assets.bitmaps.images.emplace(42,image);
    FrameSnapshot frame;
    DisplayEntry object;object.character_id=42;object.depth=1;
    object.has_matrix=true;object.matrix.translate_x=40;object.matrix.translate_y=20;
    frame.objects.push_back(object);
    assert(render_frame(frame,assets,target));
    assert(target.rgba[(1*8+2)*4]==255);
    assert(target.rgba[(1*8+2)*4+3]==255);
    auto before=target.rgba;
    frame.objects[0].character_id=999;
    assert(!render_frame(frame,assets,target));
    assert(target.rgba==before);
    frame.objects[0].character_id=42;
    // Scaling bitmap placements is supported by the affine renderer.
    frame.objects[0].matrix.scale_x=2.0;
    Surface scaled;
    assert(make_surface(8,8,scaled));
    assert(render_frame(frame,assets,scaled));
    assert(scaled.rgba[(1*8+3)*4+3]>0);
    // The previous target remains unchanged when rendering into another surface.
    assert(target.rgba==before);
    // Duplicate shape/bitmap IDs must fail without mutating previously collected assets.
    Document duplicate;
    Tag lossless;
    lossless.code=20;
    lossless.data={42,0}; // duplicate decode fails safely on truncated input
    duplicate.tags.push_back(lossless);
    FrameAssets unchanged=assets;
    assert(!collect_frame_assets(duplicate,unchanged));
    assert(unchanged.bitmaps.images.size()==assets.bitmaps.images.size());
    // Affine placement: a 90-degree rotation of the 1x1 bitmap.
    frame.objects[0].matrix.scale_x=0;
    frame.objects[0].matrix.scale_y=0;
    frame.objects[0].matrix.rotate_skew_0=-1;
    frame.objects[0].matrix.rotate_skew_1=1;
    frame.objects[0].matrix.translate_x=60;
    frame.objects[0].matrix.translate_y=20;
    Surface rotated;
    assert(make_surface(8,8,rotated));
    assert(render_frame(frame,assets,rotated));
    assert(rotated.rgba[(1*8+2)*4+3]==255);
    // Singular transforms must fail without modifying the target.
    frame.objects[0].matrix.rotate_skew_1=0;
    const auto preserved=rotated.rgba;
    assert(!render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    // Nonfinite placement matrices are rejected without touching the target.
    frame.objects[0].matrix.scale_x=std::numeric_limits<double>::infinity();
    assert(!render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    frame.objects[0].matrix.scale_x=1.0;
    frame.objects[0].matrix.rotate_skew_0=std::numeric_limits<double>::quiet_NaN();
    assert(!render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    // An invertible but strongly anisotropic bitmap matrix must be safe.
    frame.objects[0].matrix.scale_x=1e10;
    frame.objects[0].matrix.scale_y=1e-10;
    frame.objects[0].matrix.rotate_skew_0=0;
    frame.objects[0].matrix.rotate_skew_1=0;
    assert(render_frame(frame,assets,rotated));
    assert(rotated.rgba==preserved);
    // Affine Shape: scale a solid square through the frame renderer.
    ShapeGeometry square;
    square.character_id=7;
    square.fills.push_back({{255,0,0,255}});
    square.commands.push_back({ShapeCommandKind::move_to,{}, {0,0},0,0,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {40,0},0,1,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {40,40},0,1,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {0,40},0,1,0});
    square.commands.push_back({ShapeCommandKind::line_to,{}, {0,0},0,1,0});
    assets.shapes.emplace(7,square);
    FrameSnapshot shape_frame;
    DisplayEntry placed;
    placed.character_id=7;placed.depth=1;placed.has_matrix=true;
    placed.matrix.scale_x=2;placed.matrix.scale_y=2;
    shape_frame.objects.push_back(placed);
    Surface shape_surface;
    assert(make_surface(16,16,shape_surface));
    assert(render_frame(shape_frame,assets,shape_surface));
    assert(shape_surface.rgba[(1*16+1)*4+3]>0);
    // Interpolation of opaque colors at a half-pixel shift.
    Bitmap gradient;gradient.character_id=55;gradient.width=2;gradient.height=1;
    gradient.rgba={255,0,0,255,0,0,255,255};
    assets.bitmaps.images.emplace(55,gradient);
    FrameSnapshot filtered;
    DisplayEntry shifted;shifted.depth=1;shifted.character_id=55;
    shifted.has_matrix=true;shifted.matrix.translate_x=10;
    filtered.objects.push_back(shifted);
    Surface filtered_surface;
    assert(make_surface(4,4,filtered_surface));
    assert(render_frame(filtered,assets,filtered_surface));
    assert(filtered_surface.rgba[(0*4+1)*4]>0);
    assert(filtered_surface.rgba[(0*4+1)*4+2]>0);
    // A solid vector square should retain its filled interior under rotation.
    placed.matrix.scale_x=0; placed.matrix.scale_y=0;
    placed.matrix.rotate_skew_0=-1; placed.matrix.rotate_skew_1=1;
    placed.matrix.translate_x=100; placed.matrix.translate_y=0;
    shape_frame.objects[0]=placed;
    clear(shape_surface,{0,0,0,0});
    assert(render_frame(shape_frame,assets,shape_surface));
    assert(shape_surface.rgba[(1*16+4)*4+3]>0);
    // Uniform scaling preserves a stroke's circular cross section.
    ShapeGeometry stroked;
    stroked.character_id=88;
    ShapeLine line;
    line.width=20;
    line.color={10,240,20,255};
    stroked.lines.push_back(line);
    stroked.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    stroked.commands.push_back({ShapeCommandKind::line_to,{}, {100,40},0,0,1});
    assets.shapes.emplace(88,stroked);
    FrameSnapshot stroke_frame;
    DisplayEntry stroke_entry;
    stroke_entry.depth=1;stroke_entry.character_id=88;stroke_entry.has_matrix=true;
    stroke_entry.matrix.scale_x=2;stroke_entry.matrix.scale_y=2;
    stroke_frame.objects.push_back(stroke_entry);
    Surface stroke_surface;
    assert(make_surface(16,16,stroke_surface));
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+6)*4+3]>0);
    // Non-uniform scaling of a butt-capped straight stroke uses its transformed outline.
    stroked.lines[0].start_cap=1;
    stroked.lines[0].end_cap=1;
    assets.shapes[88]=stroked;
    stroke_frame.objects[0].matrix.scale_x=3;
    stroke_frame.objects[0].matrix.scale_y=1;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(2*16+9)*4+3]>0);
    // Two straight edges with butt caps can share the affine outline path.
    ShapeGeometry polyline=stroked;
    polyline.commands.push_back({ShapeCommandKind::line_to,{}, {100,120},0,0,1});
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // An anisotropically scaled polyline also supports a bevel outer join.
    polyline.lines[0].join=1;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Miter joins and limit-triggered bevel fallback use the same transformed outlines.
    polyline.lines[0].join=2;
    polyline.lines[0].miter_limit=5.0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    polyline.lines[0].miter_limit=1.0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Round joins use a tessellated circular sector under affine scaling.
    polyline.lines[0].join=0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Round and square endcaps must remain valid after nonuniform transforms.
    polyline.lines[0].start_cap=0;
    polyline.lines[0].end_cap=0;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    polyline.lines[0].start_cap=2;
    polyline.lines[0].end_cap=2;
    assets.shapes[88]=polyline;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Repeating a translucent edge must not double its opacity.
    ShapeGeometry translucent=polyline;
    translucent.lines[0].color.a=128;
    assets.shapes[88]=translucent;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    const auto alpha_once=stroke_surface.rgba[(2*16+9)*4+3];
    translucent.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    translucent.commands.push_back({ShapeCommandKind::line_to,{}, {100,40},0,0,1});
    assets.shapes[88]=translucent;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(2*16+9)*4+3]==alpha_once);
    // Oppositely traversed duplicate segments must union, not cancel.
    ShapeGeometry reversed=translucent;
    reversed.commands.push_back({ShapeCommandKind::move_to,{}, {100,40},0,0,0});
    reversed.commands.push_back({ShapeCommandKind::line_to,{}, {20,40},0,0,1});
    assets.shapes[88]=reversed;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(2*16+9)*4+3]>0);
    // Clipped contour scratch space must preserve pixels away from a local stroke.
    Surface clipped;
    assert(make_surface(256,256,clipped));
    FrameSnapshot distant;
    DisplayEntry far_entry;
    far_entry.depth=1;far_entry.character_id=88;far_entry.has_matrix=true;
    far_entry.matrix.scale_x=3;far_entry.matrix.scale_y=1;
    far_entry.matrix.translate_x=3000;far_entry.matrix.translate_y=3000;
    distant.objects.push_back(far_entry);
    assert(render_frame(distant,assets,clipped));
    assert(clipped.rgba[3]==0);
    // The union mask must composite correctly at a nonzero origin.
    Surface offset_surface;
    assert(make_surface(256,256,offset_surface));
    assert(render_frame(distant,assets,offset_surface));
    assert(offset_surface.rgba[3]==0);
    // Sparse-looking union of translucent paths uses single-channel coverage.
    Surface one_channel;
    assert(make_surface(256,256,one_channel));
    assert(render_frame(distant,assets,one_channel));
    assert(one_channel.rgba==offset_surface.rgba);
    // Distant translucent contours are composited through sparse tile storage.
    Surface sparse_union;
    assert(make_surface(256,256,sparse_union));
    assert(render_frame(distant,assets,sparse_union));
    assert(sparse_union.rgba==offset_surface.rgba);
    // A quadratic centerline can be tessellated and transformed anisotropically.
    ShapeGeometry curved_stroke=polyline;
    curved_stroke.commands.clear();
    curved_stroke.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    curved_stroke.commands.push_back({ShapeCommandKind::quadratic_to,{60,140},{100,40},0,0,1});
    assets.shapes[88]=curved_stroke;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(5*16+9)*4+3]>0);
    // Extremely curved segments must be rejected rather than silently clamped.
    ShapeGeometry extreme=curved_stroke;
    extreme.commands[1].control={0,2000000000};
    assets.shapes[88]=extreme;
    auto preserved=stroke_surface.rgba;
    assert(!render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba==preserved);
    // Tiny quadratic curves whose subsegments round to the same twip
    // must still render successfully without zero-length join geometry.
    ShapeGeometry tiny=curved_stroke;
    tiny.commands[1].control={21,41};
    tiny.commands[1].endpoint={22,40};
    assets.shapes[88]=tiny;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Explicitly closed quadratic contours join their final and first edges.
    ShapeGeometry closed_curve=curved_stroke;
    closed_curve.commands.push_back({ShapeCommandKind::quadratic_to,{20,120},{20,40},0,0,1});
    assets.shapes[88]=closed_curve;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Closing a curve must suppress its nominal square-cap extensions.
    closed_curve.lines[0].start_cap=2;
    closed_curve.lines[0].end_cap=2;
    assets.shapes[88]=closed_curve;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    closed_curve.lines[0].start_cap=0;
    closed_curve.lines[0].end_cap=0;
    assets.shapes[88]=closed_curve;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // Crossing reversed strokes should render without cancellation or repeated alpha.
    ShapeGeometry cross=translucent;
    cross.commands.clear();
    cross.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    cross.commands.push_back({ShapeCommandKind::line_to,{}, {100,120},0,0,1});
    cross.commands.push_back({ShapeCommandKind::move_to,{}, {100,40},0,0,0});
    cross.commands.push_back({ShapeCommandKind::line_to,{}, {20,120},0,0,1});
    assets.shapes[88]=cross;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Affine outline union must honor the caller's tile budget and fail atomically.
    Surface budget_target;
    assert(make_surface(256,256,budget_target));
    ShapeGeometry wide=cross;
    wide.commands.clear();
    wide.commands.push_back({ShapeCommandKind::move_to,{}, {20,800},0,0,0});
    wide.commands.push_back({ShapeCommandKind::line_to,{}, {2200,800},0,0,1});
    assets.shapes[88]=wide;
    FrameSnapshot budget_frame;
    DisplayEntry budget_entry;
    budget_entry.depth=1;budget_entry.character_id=88;budget_entry.has_matrix=true;
    budget_entry.matrix.scale_x=2;budget_entry.matrix.scale_y=1;
    budget_frame.objects.push_back(budget_entry);
    const auto pristine=budget_target.rgba;
    const auto too_small=render_frame(budget_frame,assets,budget_target,{1,4096});
    assert(!too_small && too_small.error==Error::size_limit);
    assert(budget_target.rgba==pristine);
    assert(render_frame(budget_frame,assets,budget_target,{16,65536}));
    // Multiple overlapping and reversed contours must remain stable with
    // the cached tile lookup and degenerate contour skipping.
    ShapeGeometry overlapping=cross;
    overlapping.commands.push_back({ShapeCommandKind::move_to,{}, {20,40},0,0,0});
    overlapping.commands.push_back({ShapeCommandKind::line_to,{}, {20,40},0,0,1});
    assets.shapes[88]=overlapping;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Multiple separated strokes force many sparse tiles and hash-table growth.
    ShapeGeometry spread=translucent;
    spread.commands.clear();
    for(int i=0;i<24;++i) {
        const int x=200+i*180;
        spread.commands.push_back({ShapeCommandKind::move_to,{}, {x,200},0,0,0});
        spread.commands.push_back({ShapeCommandKind::line_to,{}, {x+80,200},0,0,1});
    }
    assets.shapes[88]=spread;
    Surface spread_surface;
    assert(make_surface(512,64,spread_surface));
    FrameSnapshot spread_frame;
    DisplayEntry spread_entry;
    spread_entry.depth=1;spread_entry.character_id=88;spread_entry.has_matrix=true;
    spread_entry.matrix.scale_x=2;spread_entry.matrix.scale_y=1;
    spread_frame.objects.push_back(spread_entry);
    assert(render_frame(spread_frame,assets,spread_surface,{128,128*4096}));
    assert(spread_surface.rgba[(10*512+20)*4+3]>0);
    // Two half-pixel rectangles on opposite sides of one pixel should
    // union to full coverage rather than taking only the maximum half.
    ShapeGeometry subpixel;
    subpixel.character_id=88;
    ShapeLine subline; subline.width=10; subline.color={200,50,20,128};
    subline.start_cap=1;subline.end_cap=1;
    subpixel.lines.push_back(subline);
    subpixel.commands.push_back({ShapeCommandKind::move_to,{}, {10,0},0,0,0});
    subpixel.commands.push_back({ShapeCommandKind::line_to,{}, {10,20},0,0,1});
    subpixel.commands.push_back({ShapeCommandKind::move_to,{}, {20,0},0,0,0});
    subpixel.commands.push_back({ShapeCommandKind::line_to,{}, {20,20},0,0,1});
    assets.shapes[88]=subpixel;
    Surface samples;
    assert(make_surface(8,8,samples));
    FrameSnapshot sample_frame;
    DisplayEntry sample_entry;
    sample_entry.depth=1;sample_entry.character_id=88;sample_entry.has_matrix=true;
    sample_entry.matrix.scale_x=2;sample_entry.matrix.scale_y=1;
    sample_frame.objects.push_back(sample_entry);
    assert(render_frame(sample_frame,assets,samples));
    assert(samples.rgba[3]>0);
    // Slanted intersecting contours exercise all four scanline sample rows.
    ShapeGeometry slanted=cross;
    slanted.commands.push_back({ShapeCommandKind::move_to,{}, {30,30},0,0,0});
    slanted.commands.push_back({ShapeCommandKind::line_to,{}, {110,110},0,0,1});
    assets.shapes[88]=slanted;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Adjacent overlapping and reversed contours exercise sorted X intersections.
    ShapeGeometry winding=slanted;
    winding.commands.push_back({ShapeCommandKind::move_to,{}, {110,110},0,0,0});
    winding.commands.push_back({ShapeCommandKind::line_to,{}, {30,30},0,0,1});
    assets.shapes[88]=winding;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    assert(stroke_surface.rgba[(4*16+9)*4+3]>0);
    // Dense crossing paths exercise the large-active-edge sorting branch.
    ShapeGeometry dense=translucent;
    dense.commands.clear();
    for(int i=0;i<40;++i) {
        const int x=20+i*5;
        dense.commands.push_back({ShapeCommandKind::move_to,{}, {x,0},0,0,0});
        dense.commands.push_back({ShapeCommandKind::line_to,{}, {x+60,240},0,0,1});
    }
    assets.shapes[88]=dense;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    // A tall slanted stroke traverses multiple X-reanchoring intervals.
    ShapeGeometry tall=translucent;
    tall.commands.clear();
    tall.commands.push_back({ShapeCommandKind::move_to,{}, {400,400},0,0,0});
    tall.commands.push_back({ShapeCommandKind::line_to,{}, {900,9000},0,0,1});
    assets.shapes[88]=tall;
    Surface tall_surface;
    assert(make_surface(128,512,tall_surface));
    FrameSnapshot tall_frame;
    DisplayEntry tall_entry;
    tall_entry.depth=1;
    tall_entry.character_id=88;
    tall_entry.has_matrix=true;
    tall_entry.matrix.scale_x=2;
    tall_entry.matrix.scale_y=1;
    tall_frame.objects.push_back(tall_entry);
    assert(render_frame(tall_frame,assets,tall_surface));
    // Interpolated center: x=2*(20+(200-20)*(45-20)/(450-20)) ~= 60.93.
    assert(tall_surface.rgba[(200*128+61)*4+3]>0);
    // A contour completely outside the surface must not write any pixels.
    ShapeGeometry offscreen=stroked;
    offscreen.commands.clear();
    offscreen.commands.push_back({ShapeCommandKind::move_to,{}, {10000,10000},0,0,0});
    offscreen.commands.push_back({ShapeCommandKind::line_to,{}, {12000,14000},0,0,1});
    assets.shapes[88]=offscreen;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface));
    for(std::size_t i=3;i<stroke_surface.rgba.size();i+=4)
        assert(stroke_surface.rgba[i]==0);
    // Offscreen contours should neither change the visible stroke nor
    // consume the small sparse-mask budget.
    assets.shapes[88]=stroked;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface,{1,8192}));
    const auto visible_stroke=stroke_surface.rgba;
    ShapeGeometry combined=stroked;
    combined.commands.insert(combined.commands.end(),
                             offscreen.commands.begin(),offscreen.commands.end());
    assets.shapes[88]=combined;
    clear(stroke_surface,{0,0,0,0});
    assert(render_frame(stroke_frame,assets,stroke_surface,{1,8192}));
    assert(stroke_surface.rgba==visible_stroke);
    return 0;
}
